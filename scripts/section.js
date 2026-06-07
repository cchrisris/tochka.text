const sectionOrder = [
  { id: "fish-dishes", label: "Блюда из рыбы", path: "./fish-dishes.html" },
  { id: "chagall-walked", label: "Куда шагал Шагал?", path: "./chagall-walked.html" },
  { id: "game-over", label: "Game Over", path: "./game-over.html" },
  { id: "watch-poems", label: "Стихи смотреть онлайн", path: "./watch-poems.html" },
  { id: "quirks", label: "Чудачества", path: "./quirks.html" },
  { id: "life-how-to-live", label: "Как жить жизнь?", path: "./life-how-to-live.html" },
  { id: "who-did-this", label: "Кто все это сделал !?", path: "./who-did-this.html" },
];
document.body.classList.add(`theme-${document.body.dataset.theme ?? "fish"}`);
const titleNode = document.querySelector("#sectionTitle");
const feedNode = document.querySelector("#feed");
const headerNode = document.querySelector("#sectionHeader");
const heroTitleNode = document.querySelector("#heroTitle");
const heroImageNode = document.querySelector("#heroImage");
const sectionId = document.body.dataset.sectionId ?? "";

const appendFormattedText = (target, rawText) => {
  const parts = rawText.split(/(\*[^*\n]+\*)/g);
  for (const part of parts) {
    if (!part) {
      continue;
    }
    if (part.startsWith("*") && part.endsWith("*") && part.length > 2) {
      const em = document.createElement("em");
      em.textContent = part.slice(1, -1);
      target.append(em);
    } else {
      target.append(document.createTextNode(part));
    }
  }
};

const createPieceTextNode = (piece) => {
  if (piece.format === "silk-path") {
    const specialText = document.createElement("div");
    specialText.className = "piece-text piece-text-silk";
    let lineIndex = 0;
    for (const line of piece.text.split("\n")) {
      if (line.trim().length === 0) {
        const spacer = document.createElement("span");
        spacer.className = "piece-line piece-line-gap";
        specialText.append(spacer);
        continue;
      }
      const lineNode = document.createElement("span");
      lineNode.className = "piece-line";
      lineNode.textContent = line;
      const offset = Math.round((Math.sin(lineIndex / 1.35) + 1) * 28);
      lineNode.style.setProperty("--line-offset", `${offset}px`);
      lineIndex += 1;
      specialText.append(lineNode);
    }
    return specialText;
  }

  const prose = document.createElement("p");
  prose.className = "piece-text piece-text-raw";

  if (!piece.inlineImages || piece.inlineImages.length === 0) {
    appendFormattedText(prose, piece.text);
    return prose;
  }

  const wrapper = document.createElement("div");
  wrapper.className = "piece-text-inline-wrap";

  let remainder = piece.text;
  for (const inlineImage of piece.inlineImages) {
    const markerIndex = remainder.indexOf(inlineImage.after);
    if (markerIndex === -1) {
      continue;
    }

    const endIndex = markerIndex + inlineImage.after.length;
    const textPart = remainder.slice(0, endIndex);
    if (textPart.length > 0) {
      const partNode = document.createElement("p");
      partNode.className = "piece-text piece-text-raw";
      appendFormattedText(partNode, textPart);
      wrapper.append(partNode);
    }

    const imageNode = document.createElement("img");
    imageNode.className = "piece-inline-image";
    imageNode.src = inlineImage.src;
    imageNode.alt = inlineImage.alt ?? "Иллюстрация внутри произведения";
    imageNode.loading = "lazy";
    wrapper.append(imageNode);

    remainder = remainder.slice(endIndex);
  }

  if (remainder.length > 0) {
    const tailNode = document.createElement("p");
    tailNode.className = "piece-text piece-text-raw";
    appendFormattedText(tailNode, remainder);
    wrapper.append(tailNode);
  }

  return wrapper;
};

const renderSection = (pageData) => {
  if (titleNode) {
    titleNode.textContent = pageData.sectionName;
  }

  if (heroTitleNode) {
    heroTitleNode.textContent = pageData.sectionName;
  }

  if (heroImageNode) {
    heroImageNode.src = pageData.heroImage ?? "../pix/rht_logo.png";
    heroImageNode.alt = pageData.heroImageAlt ?? `Раздел ${pageData.sectionName}`;
  }

  if (!feedNode) {
    return;
  }

  feedNode.innerHTML = "";

  for (const piece of pageData.pieces) {
    const article = document.createElement("article");
    article.className = "piece";
    const isImageOnly = piece.text.trim().length === 0 && Boolean(piece.image);
    const isCredits = pageData.layout === "credits";
    if (isImageOnly) {
      article.classList.add("piece-image-only");
    }
    if (isCredits) {
      article.classList.add("piece-credits");
    }

    const main = document.createElement("div");
    main.className = "piece-main";
    if (isCredits) {
      main.classList.add("piece-main-credits");
    }
    if (!isImageOnly) {
      main.append(createPieceTextNode(piece));
    }

    if (piece.image) {
      const image = document.createElement("img");
      image.className = "piece-main-image";
      image.src = piece.image;
      image.alt = piece.imageAlt ?? "Иллюстрация к произведению";
      image.loading = "lazy";
      main.append(image);
    }

    article.append(main);
    if (!isCredits) {
      const side = document.createElement("aside");
      side.className = "piece-side";

      const author = document.createElement("p");
      author.className = "piece-side-value";
      author.textContent = piece.author;

      const title = document.createElement("p");
      title.className = "piece-side-value piece-side-title";
      title.textContent = piece.title;

      side.append(author, title);
      article.append(side);
    }
    feedNode.append(article);
  }
};

const createNavItem = (item, direction) => {
  const wrapper = document.createElement("div");
  wrapper.className = "section-nav-item";

  if (!item) {
    const disabled = document.createElement("span");
    disabled.className = "section-nav-link is-disabled";
    disabled.textContent = direction === "prev" ? "←" : "→";
    wrapper.append(disabled);
    return wrapper;
  }

  const link = document.createElement("a");
  link.className = "section-nav-link";
  link.href = item.path;
  link.setAttribute(
    "aria-label",
    direction === "prev" ? `Предыдущий: ${item.label}` : `Следующий: ${item.label}`,
  );

  const arrow = document.createElement("span");
  arrow.className = "section-nav-arrow";
  arrow.textContent = direction === "prev" ? "←" : "→";

  const label = document.createElement("span");
  label.className = "section-nav-label";
  label.textContent = item.label;

  if (direction === "prev") {
    link.append(arrow, label);
  } else {
    link.append(label, arrow);
  }

  wrapper.append(link);
  return wrapper;
};

const renderBottomNavigation = () => {
  const currentIndex = sectionOrder.findIndex((item) => item.id === sectionId);
  if (currentIndex === -1 || !feedNode) {
    return;
  }

  const nav = document.createElement("nav");
  nav.className = "section-nav";
  nav.setAttribute("aria-label", "Навигация по разделам");

  const prev = sectionOrder[currentIndex - 1];
  const next = sectionOrder[currentIndex + 1];

  nav.append(createNavItem(prev, "prev"), createNavItem(next, "next"));
  feedNode.insertAdjacentElement("afterend", nav);
};

let lastY = 0;
let ticking = false;
let shown = false;
const revealOffset = 80;
headerNode?.classList.add("hidden");

const updateHeader = () => {
  const y = window.scrollY;
  const feedTop = feedNode
    ? feedNode.getBoundingClientRect().top + window.scrollY
    : window.innerHeight;
  const showThreshold = Math.max(60, feedTop - revealOffset);

  if (!shown && y >= showThreshold) {
    shown = true;
  }
  if (shown) {
    headerNode?.classList.remove("hidden");
  } else {
    headerNode?.classList.add("hidden");
  }
  lastY = y;
  ticking = false;
};

window.addEventListener("scroll", () => {
  if (!ticking) {
    window.requestAnimationFrame(updateHeader);
    ticking = true;
  }
});

const loadSectionData = async () => {
  if (!sectionId) {
    throw new Error("Отсутствует идентификатор раздела.");
  }

  const cacheKey = "tochka.sections.v1";
  let data = null;

  const cached = sessionStorage.getItem(cacheKey);
  if (cached) {
    try {
      data = JSON.parse(cached);
    } catch {
      sessionStorage.removeItem(cacheKey);
    }
  }

  if (!data) {
    const response = await fetch("../content/sections.json");
    if (!response.ok) {
      throw new Error("Не удалось загрузить данные разделов.");
    }
    data = await response.json();
    sessionStorage.setItem(cacheKey, JSON.stringify(data));
  }

  const sectionData = data.sections[sectionId];
  if (!sectionData) {
    throw new Error(`Раздел "${sectionId}" не найден.`);
  }

  renderSection(sectionData);
  renderBottomNavigation();
};

loadSectionData().catch((error) => {
  console.error(error);
});
