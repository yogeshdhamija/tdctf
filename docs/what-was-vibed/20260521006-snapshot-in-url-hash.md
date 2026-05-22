# Move the URL snapshot from `?s=` to `#s=`

## Motivation

`index.html` is the whole game (wasm + JS base64-embedded in a single
file). When the snapshot lived in the query string, every state change
produced a brand-new URL (`/index.html?s=v1~T0~PR...`), and HTTP caches
key on the full URL — so a CDN or browser cache would treat each
snapshot as a different resource and re-fetch the entire ~1 MB document.

The fragment (everything after `#`) is **not** sent to the server and
is not part of the HTTP cache key. Moving the snapshot into the hash
means there is exactly one cacheable URL for the document — the browser
fetches `index.html` once and the only thing that changes per click is
the in-page state.

## Change

`src/platform/platform_web.c`, the three `EM_JS` URL helpers:

- `js_push_snapshot` / `js_replace_snapshot`: write to `url.hash`
  (`"s=" + snapshot`) instead of `url.searchParams.set("s", …)`. Still
  go through `history.pushState` / `replaceState` so popstate keeps
  firing for back/forward.
- `js_read_url_snapshot`: read `window.location.hash`, strip the
  leading `#`, require the `s=` prefix, copy the rest into the wasm
  buffer.

The leading comment on the section was updated to explain the
fragment-vs-query choice.

## Notes

- Snapshot characters (`v T P R B W F N`, digits, `~ : , -`) are all
  allowed in URL fragments per RFC 3986, so no percent-encoding is
  needed and the URL stays human-readable in the address bar.
- `pushState`/`replaceState` on a hash-only change fires `popstate` on
  back/forward (same as before); we don't rely on `hashchange`.
- Old `?s=…` links won't auto-migrate — the read path now only looks
  at the fragment. If we ever need to honour legacy links, fall back
  to the query param when the hash is absent.
