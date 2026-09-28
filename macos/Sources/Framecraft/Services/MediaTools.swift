import AppKit
import AVFoundation
import FramecraftCore
import ImageIO
import SwiftUI
import UniformTypeIdentifiers

enum MediaTools {
    /// Duration in seconds of a video or audio file, or nil when AVFoundation cannot read it.
    static func duration(of url: URL) async -> Double? {
        let asset = AVURLAsset(url: url)
        guard let time = try? await asset.load(.duration) else { return nil }
        let seconds = CMTimeGetSeconds(time)
        return seconds.isFinite && seconds > 0 ? seconds : nil
    }

    /// The last frame of a video (to chain clips: "@Image IS the first frame").
    static func lastFrame(of url: URL) async throws -> CGImage {
        let asset = AVURLAsset(url: url)
        let duration = try await asset.load(.duration)
        let generator = AVAssetImageGenerator(asset: asset)
        generator.appliesPreferredTrackTransform = true
        generator.requestedTimeToleranceBefore = .zero
        generator.requestedTimeToleranceAfter = .zero
        let end = CMTimeSubtract(duration, CMTime(value: 1, timescale: 20))
        return try await generator.image(at: CMTimeMaximum(end, .zero)).image
    }

    static func pngData(_ image: CGImage) -> Data? {
        let data = NSMutableData()
        guard let destination = CGImageDestinationCreateWithData(data, UTType.png.identifier as CFString, 1, nil) else { return nil }
        CGImageDestinationAddImage(destination, image, nil)
        return CGImageDestinationFinalize(destination) ? data as Data : nil
    }

    static func imageThumbnail(_ url: URL, maxPixel: Int) -> CGImage? {
        guard let source = CGImageSourceCreateWithURL(url as CFURL, nil) else { return nil }
        let options: [CFString: Any] = [
            kCGImageSourceCreateThumbnailFromImageAlways: true,
            kCGImageSourceCreateThumbnailWithTransform: true,
            kCGImageSourceThumbnailMaxPixelSize: maxPixel,
        ]
        return CGImageSourceCreateThumbnailAtIndex(source, 0, options as CFDictionary)
    }

    static func videoThumbnail(_ url: URL, maxPixel: Int) async -> CGImage? {
        let generator = AVAssetImageGenerator(asset: AVURLAsset(url: url))
        generator.appliesPreferredTrackTransform = true
        generator.maximumSize = CGSize(width: maxPixel, height: maxPixel)
        return try? await generator.image(at: CMTime(value: 1, timescale: 2)).image
    }

    static func pixelSize(of url: URL) -> CGSize? {
        guard let source = CGImageSourceCreateWithURL(url as CFURL, nil),
              let properties = CGImageSourceCopyPropertiesAtIndex(source, 0, nil) as? [CFString: Any],
              let width = properties[kCGImagePropertyPixelWidth] as? Int,
              let height = properties[kCGImagePropertyPixelHeight] as? Int
        else { return nil }
        return CGSize(width: width, height: height)
    }

    static func formattedDuration(_ seconds: Double) -> String {
        seconds < 10 ? String(format: "%.1f s", seconds) : "\(Int(seconds.rounded())) s"
    }
}

/// Carries a CGImage out of a detached task.
private struct ImageBox: @unchecked Sendable {
    let image: CGImage?
}

/// In-memory thumbnail cache shared by all views.
final class ThumbnailCache: @unchecked Sendable {
    static let shared = ThumbnailCache()
    private let cache = NSCache<NSString, NSImage>()

    func load(_ url: URL, video: Bool, maxPixel: Int) async -> NSImage? {
        let key = "\(url.path)#\(maxPixel)" as NSString
        if let hit = cache.object(forKey: key) { return hit }
        let cgImage: CGImage?
        if video {
            cgImage = await MediaTools.videoThumbnail(url, maxPixel: maxPixel)
        } else {
            cgImage = await Task.detached(priority: .userInitiated) {
                ImageBox(image: MediaTools.imageThumbnail(url, maxPixel: maxPixel))
            }.value.image
        }
        guard let cgImage else { return nil }
        let image = NSImage(cgImage: cgImage, size: NSSize(width: cgImage.width, height: cgImage.height))
        cache.setObject(image, forKey: key)
        return image
    }
}

/// Async thumbnail for a local image or video file.
struct Thumbnail: View {
    let url: URL?
    var video = false
    var maxPixel = 640
    var contentMode: ContentMode = .fill

    @State private var image: NSImage?
    @State private var failed = false

    var body: some View {
        ZStack {
            if let image {
                Image(nsImage: image)
                    .resizable()
                    .aspectRatio(contentMode: contentMode)
                    .transition(.opacity)
            } else {
                Rectangle().fill(Color.primary.opacity(0.06))
                if failed {
                    Image(systemName: video ? "film" : "photo")
                        .font(.title2)
                        .foregroundStyle(.secondary)
                } else {
                    ProgressView().controlSize(.small)
                }
            }
        }
        .task(id: url) {
            image = nil
            failed = false
            guard let url else {
                failed = true
                return
            }
            let loaded = await ThumbnailCache.shared.load(url, video: video, maxPixel: maxPixel)
            withAnimation(.easeOut(duration: 0.2)) { image = loaded }
            failed = loaded == nil
        }
    }
}

// MARK: - Story final cut

extension MediaTools {
    /// Joins clips one after another (video + audio) into a single MP4.
    static func concatenate(_ clips: [URL], to destination: URL) async throws {
        let composition = AVMutableComposition()
        guard let videoTrack = composition.addMutableTrack(withMediaType: .video, preferredTrackID: kCMPersistentTrackID_Invalid) else {
            throw KieError("No se pudo preparar el montaje.", definite: true)
        }
        let audioTrack = composition.addMutableTrack(withMediaType: .audio, preferredTrackID: kCMPersistentTrackID_Invalid)
        var cursor = CMTime.zero
        var transformSet = false
        for clip in clips {
            let asset = AVURLAsset(url: clip)
            let duration = try await asset.load(.duration)
            let range = CMTimeRange(start: .zero, duration: duration)
            guard let sourceVideo = try await asset.loadTracks(withMediaType: .video).first else {
                throw KieError("\(clip.lastPathComponent) no tiene imagen.", definite: true)
            }
            try videoTrack.insertTimeRange(range, of: sourceVideo, at: cursor)
            if !transformSet {
                videoTrack.preferredTransform = try await sourceVideo.load(.preferredTransform)
                transformSet = true
            }
            if let sourceAudio = try await asset.loadTracks(withMediaType: .audio).first {
                try audioTrack?.insertTimeRange(range, of: sourceAudio, at: cursor)
            }
            cursor = CMTimeAdd(cursor, duration)
        }
        try? FileManager.default.removeItem(at: destination)
        guard let export = AVAssetExportSession(asset: composition, presetName: AVAssetExportPresetHighestQuality) else {
            throw KieError("No se pudo exportar el video.", definite: true)
        }
        export.outputURL = destination
        export.outputFileType = .mp4
        export.shouldOptimizeForNetworkUse = true
        let box = ExportBox(session: export)
        try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<Void, Error>) in
            box.session.exportAsynchronously {
                if box.session.status == .completed {
                    continuation.resume()
                } else {
                    continuation.resume(throwing: box.session.error ?? KieError("La exportación se canceló.", definite: true))
                }
            }
        }
    }
}

/// Lets the export session cross into the completion handler.
private struct ExportBox: @unchecked Sendable {
    let session: AVAssetExportSession
}
