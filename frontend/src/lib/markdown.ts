// Превращает markdown в простой текст для превью в карточках/подборках:
// убирает картинки, ссылки и базовую разметку, оставляя читаемый текст.
export function markdownToPlain(md: string): string {
  return md
    .replace(/!\[[^\]]*\]\([^)]*\)/g, "") // картинки
    .replace(/\[([^\]]*)\]\([^)]*\)/g, "$1") // ссылки -> текст
    .replace(/^#{1,6}\s+/gm, "") // заголовки
    .replace(/^>\s?/gm, "") // цитаты
    .replace(/[*_`~]/g, "") // эмфазис/код
    .replace(/\n{3,}/g, "\n\n")
    .trim();
}

export function excerpt(text: string, max = 360): string {
  const plain = markdownToPlain(text);
  if (plain.length <= max) return plain;
  return plain.slice(0, max).trimEnd() + "…";
}
