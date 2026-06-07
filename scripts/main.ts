export {};

type SearchItem = {
  label: string;
  path: string;
};

const searchItems: SearchItem[] = [
  { label: "Блюда из рыбы", path: "./sections/fish-dishes.html" },
  { label: "Куда шагал Шагал?", path: "./sections/chagall-walked.html" },
  { label: "Game Over", path: "./sections/game-over.html" },
  { label: "Стихи смотреть онлайн", path: "./sections/watch-poems.html" },
  { label: "Чудачества", path: "./sections/quirks.html" },
  { label: "Как жить жизнь?", path: "./sections/life-how-to-live.html" },
  { label: "Кто все это сделал !?", path: "./sections/who-did-this.html" },
];

const searchInput = document.querySelector<HTMLInputElement>("#searchInput");
const searchForm = document.querySelector<HTMLFormElement>("#searchForm");
const suggestions = document.querySelector<HTMLUListElement>("#suggestions");
const aboutButton = document.querySelector<HTMLButtonElement>("#aboutButton");
const aboutModal = document.querySelector<HTMLDivElement>("#aboutModal");
const closeAboutButton = document.querySelector<HTMLButtonElement>("#closeAbout");
const defaultSearchHint = "Литературный альманах";

const syncSearchHintState = (): void => {
  if (!searchInput) {
    return;
  }
  if (searchInput.value.trim() === defaultSearchHint) {
    searchInput.classList.add("is-hint");
    return;
  }
  searchInput.classList.remove("is-hint");
};

const renderSuggestions = (): void => {
  if (!suggestions) {
    return;
  }

  suggestions.innerHTML = "";

  for (const item of searchItems) {
    const li = document.createElement("li");
    li.className = "suggestions-item";

    const link = document.createElement("a");
    link.className = "suggestions-link";
    link.href = item.path;
    link.setAttribute("role", "option");
    link.textContent = item.label;

    li.append(link);
    suggestions.append(li);
  }
};

const openSuggestions = (): void => suggestions?.classList.add("active");
const closeSuggestions = (): void => suggestions?.classList.remove("active");

const openModal = (): void => {
  if (!aboutModal) {
    return;
  }

  aboutModal.classList.add("open");
  aboutModal.setAttribute("aria-hidden", "false");
};

const closeModal = (): void => {
  if (!aboutModal) {
    return;
  }

  aboutModal.classList.remove("open");
  aboutModal.setAttribute("aria-hidden", "true");
};

renderSuggestions();
syncSearchHintState();

searchInput?.addEventListener("focus", () => {
  openSuggestions();
});

searchInput?.addEventListener("click", openSuggestions);
searchInput?.addEventListener("input", syncSearchHintState);

document.addEventListener("click", (event) => {
  const target = event.target as Node;
  if (!searchForm?.contains(target)) {
    closeSuggestions();
  }
});

searchForm?.addEventListener("submit", (event) => {
  event.preventDefault();
  window.location.href = searchItems[0].path;
});

aboutButton?.addEventListener("click", openModal);
closeAboutButton?.addEventListener("click", closeModal);

aboutModal?.addEventListener("click", (event) => {
  if (event.target === aboutModal) {
    closeModal();
  }
});

window.addEventListener("keydown", (event) => {
  if (event.key === "Escape") {
    closeModal();
    closeSuggestions();
  }
});
