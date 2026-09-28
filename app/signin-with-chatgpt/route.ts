import { LOCAL_AUTH_COOKIE, isDesktopRequest } from "@/app/chatgpt-auth";

export const dynamic = "force-dynamic";

export async function GET(request: Request) {
  const url = new URL(request.url);
  const desktop = isDesktopRequest(request.headers, url.hostname);
  if (!desktop) return new Response("Not found", { status: 404 });

  const returnTo = safeReturn(url.searchParams.get("return_to"));
  return new Response(null, {
    status: 303,
    headers: {
      Location: returnTo,
      "Cache-Control": "private, no-store",
      "Set-Cookie": `${LOCAL_AUTH_COOKIE}=1; Path=/; HttpOnly; SameSite=Lax; Max-Age=31536000`,
    },
  });
}

function safeReturn(value: string | null) {
  if (!value?.startsWith("/") || value.startsWith("//")) return "/";
  try {
    const target = new URL(value, "http://localhost");
    if (target.origin !== "http://localhost") return "/";
    if (["/signin-with-chatgpt", "/signout-with-chatgpt", "/callback"].includes(target.pathname)) return "/";
    return `${target.pathname}${target.search}${target.hash}`;
  } catch {
    return "/";
  }
}
