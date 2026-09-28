import { LOCAL_AUTH_COOKIE } from "@/app/chatgpt-auth";

export const dynamic = "force-dynamic";

export async function GET(request: Request) {
  const url = new URL(request.url);
  const desktop =
    request.headers.get("x-framecraft-desktop") === "1" &&
    (url.hostname === "127.0.0.1" || url.hostname === "localhost");
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
