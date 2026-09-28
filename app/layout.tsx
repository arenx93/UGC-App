import type { Metadata, Viewport } from "next";
import { headers } from "next/headers";
import "./globals.css";

export const metadata: Metadata = {
  title: "Framecraft Studio",
  description: "Your KIE image and video studio for UGC content with skill-guided prompts.",
  manifest: "/manifest.webmanifest",
  appleWebApp: { capable: true, title: "Framecraft", statusBarStyle: "black-translucent" },
  other: {
    "codex-preview": "development",
  },
  icons: {
    icon: "/favicon.svg",
    shortcut: "/favicon.svg",
  },
};

export const viewport: Viewport = {
  themeColor: "#0b0a12",
  colorScheme: "dark",
};

const PLATFORMS = new Set(["mac", "win", "linux"]);

export default async function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  // The desktop shell (desktop/main.cjs) tags its requests so the UI can adapt
  // to the native title bar and translucent window materials.
  const requestHeaders = await headers();
  const desktop = requestHeaders.get("x-framecraft-desktop") === "1";
  const platform = requestHeaders.get("x-framecraft-platform") ?? "";
  const material = requestHeaders.get("x-framecraft-material") === "1";

  return (
    <html
      lang="en"
      data-desktop={desktop ? "" : undefined}
      data-platform={desktop && PLATFORMS.has(platform) ? platform : undefined}
      data-material={desktop && material ? "" : undefined}
    >
      <body className="antialiased">
        <div className="aurora" aria-hidden="true" />
        {children}
      </body>
    </html>
  );
}
