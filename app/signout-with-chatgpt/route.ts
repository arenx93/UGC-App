import { LOCAL_AUTH_COOKIE, isDesktopRequest } from "@/app/chatgpt-auth";

export const dynamic = "force-dynamic";

export async function GET(request: Request) {
  const url = new URL(request.url);
  const desktop = isDesktopRequest(request.headers, url.hostname);
  if (!desktop) return new Response("Not found", { status: 404 });
  return new Response(null, {
    status: 303,
    headers: {
      Location: "/",
      "Cache-Control": "private, no-store",
      "Set-Cookie": `${LOCAL_AUTH_COOKIE}=; Path=/; HttpOnly; SameSite=Lax; Max-Age=0`,
    },
  });
}
