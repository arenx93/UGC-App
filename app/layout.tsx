import type { Metadata } from "next";
import "./globals.css";

export const metadata: Metadata = {
  title: "Framecraft Studio",
  description: "Your KIE image and video studio with skill-guided prompts.",
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

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  return (
    <html lang="en">
      <body className="antialiased">{children}</body>
    </html>
  );
}
