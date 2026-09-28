import type { NextConfig } from "next";

const nextConfig: NextConfig = {
  experimental: {
    serverActions: {
      // Upload routes enforce their own per-media limits. This outer limit
      // allows the largest supported reference video through to the route.
      bodySizeLimit: "210mb",
    },
  },
};

export default nextConfig;
