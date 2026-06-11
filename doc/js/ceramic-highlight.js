const base = new URL("../assets/", import.meta.url).href;

const [grammar, themeLight] = await Promise.all([
  fetch(new URL("ceramic.tmLanguage.json", base)).then((r) => {
    if (!r.ok) throw new Error(`grammar HTTP ${r.status}`);
    return r.json();
  }),
  fetch(new URL("ceramic-light.json", base)).then((r) => {
    if (!r.ok) throw new Error(`theme-light HTTP ${r.status}`);
    return r.json();
  }),
]);

grammar.name = "ceramic";

const shiki = await import("https://esm.sh/shiki@1.22.0");
const highlighter = await shiki.createHighlighter({
  themes: [themeLight],
  langs: [grammar],
});

let pending = false;

const apply = () => {
  if (pending) return;
  pending = true;
  requestAnimationFrame(() => {
    pending = false;

    const candidates = [
      ...document.querySelectorAll("pre > code.language-ceramic"),
      ...document.querySelectorAll("pre[data-shiki-raw] > code"),
    ];

    for (const code of candidates) {
      const pre = code.parentElement;
      if (pre.dataset.shikiTheme === themeLight.name) continue;
      const raw = pre.dataset.shikiRaw || code.textContent;
      const html = highlighter.codeToHtml(raw, {
        lang: "ceramic",
        theme: themeLight.name,
      });
      const temp = document.createElement("div");
      temp.innerHTML = html;
      const newPre = temp.querySelector("pre");

      pre.classList.add(...newPre.classList);
      const style = newPre.getAttribute("style");
      if (style) pre.setAttribute("style", style);
      pre.dataset.shikiTheme = themeLight.name;
      pre.dataset.shikiRaw = raw;
      code.replaceWith(newPre.querySelector("code"));
    }
  });
};

if (document.readyState === "loading") {
  document.addEventListener("DOMContentLoaded", apply);
} else {
  apply();
}
