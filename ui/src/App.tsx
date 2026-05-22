import { createSignal, createEffect, For, Show, onMount, onCleanup } from 'solid-js'
import { Portal } from 'solid-js/web';
import './app.css'

interface TrackedStat {
    id: string;
    name: string;
    desc: string;
    value: number;
    isCustomRule: boolean;
    isActive: boolean;
    category: string;
    categoryRaw?: string;
}

const defaultVanillaCategories: Record<string, string> = {
    "Locations Discovered": "General", "Dungeons Cleared": "General", "Days Passed": "General",
    "Hours Slept": "General", "Hours Waiting": "General", "Standing Stones Found": "General",
    "Gold Found": "General", "Most Gold Carried": "General", "Chests Looted": "General",
    "Skill Increases": "General", "Skill Books Read": "General", "Food Eaten": "General",
    "Training Sessions": "General", "Books Read": "General", "Horses Owned": "General",
    "Houses Owned": "General", "Stores Invested In": "General", "Barters": "General",
    "Persuasions": "General", "Bribes": "General", "Intimidations": "General",

    "People Killed": "Combat", "Animals Killed": "Combat", "Creatures Killed": "Combat",
    "Undead Killed": "Combat", "Daedra Killed": "Combat", "Automatons Killed": "Combat",
    "Favorite Weapon": "Combat", "Critical Strikes": "Combat", "Sneak Attacks": "Combat",
    "Backstabs": "Combat", "Weapons Disarmed": "Combat", "Brawls Won": "Combat",
    "Bunnies Slaughtered": "Combat",

    "Locks Picked": "Crime", "Pockets Picked": "Crime", "Items Pickpocketed": "Crime",
    "Times Jailed": "Crime", "Days Jailed": "Crime", "Fines Paid": "Crime",
    "Jail Escapes": "Crime", "Items Stolen": "Crime", "Assaults": "Crime",
    "Murders": "Crime", "Horses Stolen": "Crime", "Trespasses": "Crime",

    "Eastmarch Bounty": "Bounties", "Falkreath Bounty": "Bounties", "Haafingar Bounty": "Bounties",
    "Hjaalmarch Bounty": "Bounties", "The Pale Bounty": "Bounties", "The Reach Bounty": "Bounties",
    "The Rift Bounty": "Bounties", "Tribal Orcs Bounty": "Bounties", "Whiterun Bounty": "Bounties",
    "Winterhold Bounty": "Bounties", "Total Lifetime Bounty": "Bounties", "Largest Bounty": "Bounties",

    "Soul Gems Used": "Magic", "Souls Trapped": "Magic", "Spells Learned": "Magic",
    "Favorite Spell": "Magic", "Favorite School": "Magic", "Dragon Souls Collected": "Magic",

    "Words Of Power Learned": "Shouts", "Words Of Power Unlocked": "Shouts",
    "Shouts Learned": "Shouts", "Shouts Unlocked": "Shouts", "Shouts Mastered": "Shouts",
    "Times Shouted": "Shouts", "Favorite Shout": "Shouts",

    "Weapons Improved": "Crafting", "Weapons Made": "Crafting", "Armor Improved": "Crafting",
    "Armor Made": "Crafting", "Potions Mixed": "Crafting", "Potions Used": "Crafting",
    "Poisons Mixed": "Crafting", "Poisons Used": "Crafting", "Ingredients Harvested": "Crafting",
    "Ingredients Eaten": "Crafting", "Magic Items Made": "Crafting",

    "Quests Completed": "Quests", "Misc Objectives Completed": "Quests",
    "Main Quests Completed": "Quests", "Side Quests Completed": "Quests",
    "The Companions Quests Completed": "Quests", "College of Winterhold Quests Completed": "Quests",
    "Thieves' Guild Quests Completed": "Quests", "The Dark Brotherhood Quests Completed": "Quests",
    "Civil War Quests Completed": "Quests", "Daedric Quests Completed": "Quests",
    "Dawnguard Quests Completed": "Quests", "Dragonborn Quests Completed": "Quests",
    "Questlines Completed": "Quests",

    "Diseases Contracted": "DLC", "Days as a Vampire": "DLC", "Days as a Werewolf": "DLC",
    "Necks Bitten": "DLC", "Vampirism Cures": "DLC", "Werewolf Transformations": "DLC", "Mauls": "DLC"
};

function App() {
    const [stats, setStats] = createSignal<TrackedStat[]>([]);
    const [activeTab, setActiveTab] = createSignal<string>('All');
    const [search, setSearch] = createSignal("");
    const [categoriesList, setCategoriesList] = createSignal<string[]>(["General", "Combat", "Magic", "Crime", "Bounties", "Crafting", "Shouts", "Quests", "DLC"]);
    const [newCatName, setNewCatName] = createSignal("");

    // Dicionário Dinâmico reativo alimentado via C++
    const [translations, setTranslations] = createSignal<Record<string, string>>({});

    // Sistema Macro de Tradução e Fallback
    const t = (key: string, fallback: string = "") => translations()[key] || fallback;

    const localizeCategory = (cat: string) => {
        if (!cat) return "";
        if (cat === "All") return t("ui.all_categories", "All Categories");

        // 1. Tenta correspondência direta (se a chave do cache for exatamente o token bruto enviado)
        if (translations()[cat]) return translations()[cat];

        // 2. Limpa sintaxes de interpolação como {{$key}} ou $key para buscar no cache de idiomas
        const cleanKey = cat.replace(/[\{\}\$]/g, "").trim();
        if (translations()[cleanKey]) return translations()[cleanKey];

        // 3. Fallback clássico para as chaves nativas/vanilla
        const vanillaKey = `categories.${cat}`;
        if (translations()[vanillaKey]) return translations()[vanillaKey];

        return cat;
    };

    // CORREÇÃO: Protege apenas as chaves nativas essenciais do Vanilla Skyrim.
    // Categorias customizadas ou tokens de tradução agora estão liberados para renomear/deletar.
    const isSystemCategory = (cat: string) => {
        const defaultKeys = ["General", "Combat", "Magic", "Crime", "Bounties", "Crafting", "Shouts", "Quests", "DLC"];
        return defaultKeys.includes(cat);
    };

    const [isMainCatOpen, setIsMainCatOpen] = createSignal(false);
    const [isSettingsOpen, setIsSettingsOpen] = createSignal(false);
    const [isSettingsCatOpen, setIsSettingsCatOpen] = createSignal(false);
    const [isSettingsVisOpen, setIsSettingsVisOpen] = createSignal(false);
    const [openRowDropdownId, setOpenRowDropdownId] = createSignal<string | null>(null);

    const [settingsSearch, setSettingsSearch] = createSignal("");
    const [settingsCatFilter, setSettingsCatFilter] = createSignal("All");
    const [settingsVisFilter, setSettingsVisFilter] = createSignal("All");

    const [showTooltips, setShowTooltips] = createSignal(true);
    const [hoveredDesc, setHoveredDesc] = createSignal<string | null>(null);
    const [mousePos, setMousePos] = createSignal({ x: 0, y: 0 });

    const [sortBy, setSortBy] = createSignal<'category' | 'name' | 'value'>('name');
    const [sortDirection, setSortDirection] = createSignal<'asc' | 'desc'>('asc');

    onMount(() => {
        const handleStatsReceived = (e: any) => {
            const { stats: incoming, customCategories, localization } = e.detail;

            // Define e atualiza as traduções recebidas dinamicamente do JSON
            if (localization) {
                setTranslations(localization);
            }

            const defaultKeys = ["General", "Combat", "Magic", "Crime", "Bounties", "Crafting", "Shouts", "Quests", "DLC"];
            let baseCats = [...defaultKeys];

            if (Array.isArray(customCategories) && customCategories.length > 0) {
                baseCats = customCategories;
            }

            const categoryRenameMap: Record<string, string> = {};
            defaultKeys.forEach((originalKey, index) => {
                if (baseCats[index]) {
                    categoryRenameMap[originalKey] = baseCats[index];
                }
            });

            const processed = incoming.map((stat: TrackedStat) => {
                if (!stat.isCustomRule && defaultVanillaCategories[stat.id]) {
                    if (!stat.category || stat.category.trim() === "") {
                        stat.category = defaultVanillaCategories[stat.id];
                    }
                }
                if (!stat.category || stat.category.trim() === "") {
                    stat.category = "General";
                }

                if (categoryRenameMap[stat.category]) {
                    stat.category = categoryRenameMap[stat.category];
                }

                stat.categoryRaw = stat.category;
                return stat;
            });

            const runtimeCats = new Set([...baseCats, ...processed.map((s: TrackedStat) => s.category)]);
            setCategoriesList(Array.from(runtimeCats));
            setStats(processed);
        };

        window.addEventListener('OnTrackedStatsReceived', handleStatsReceived);

        const requestDataFromBackend = () => {
            if (typeof (window as any).RequestTrackedStats === 'function') {
                (window as any).RequestTrackedStats("{}");
            }
        };

        const handleGlobalClick = () => {
            setIsMainCatOpen(false);
            setIsSettingsCatOpen(false);
            setIsSettingsVisOpen(false);
            setOpenRowDropdownId(null);
        };
        window.addEventListener('click', handleGlobalClick);

        window.addEventListener('BackendReady', requestDataFromBackend);
        const fallbackTimer = setTimeout(requestDataFromBackend, 500);

        onCleanup(() => {
            window.removeEventListener('OnTrackedStatsReceived', handleStatsReceived);
            window.removeEventListener('BackendReady', requestDataFromBackend);
            window.removeEventListener('click', handleGlobalClick);
            clearTimeout(fallbackTimer);
        });
    });

    const saveAllChangesAndClose = () => {
        const payload: any[] = [];
        payload.push({
            id: "__CustomCategories__",
            isActive: true,
            category: JSON.stringify(categoriesList())
        });

        stats().forEach(stat => {
            const currentCat = stat.categoryRaw || stat.category;
            payload.push({
                id: stat.id,
                isActive: stat.isActive,
                category: currentCat
            });
        });

        if (typeof (window as any).UpdateStatUISettings === 'function') {
            (window as any).UpdateStatUISettings(JSON.stringify(payload));
        }
        setIsSettingsOpen(false);
    };

    const addCategory = () => {
        const name = newCatName().trim();
        if (name && !categoriesList().includes(name)) {
            setCategoriesList([...categoriesList(), name]);
            setNewCatName("");
        }
    };

    const deleteCategory = (catToDelete: string) => {
        if (isSystemCategory(catToDelete)) return;
        setCategoriesList(categoriesList().filter(c => c !== catToDelete));
        setStats(prev => prev.map(s => {
            const currentCat = s.categoryRaw || s.category;
            if (currentCat === catToDelete) {
                return { ...s, category: "General", categoryRaw: "General" };
            }
            return s;
        }));
        if (activeTab() === catToDelete) setActiveTab("General");
    };

    const renameCategory = (oldName: string, newName: string) => {
        const trimmed = newName.trim();
        if (!trimmed || oldName === trimmed || isSystemCategory(oldName) || categoriesList().includes(trimmed)) return;

        setCategoriesList(categoriesList().map(c => c === oldName ? trimmed : c));
        setStats(prev => prev.map(s => {
            const currentCat = s.categoryRaw || s.category;
            if (currentCat === oldName) {
                return { ...s, category: trimmed, categoryRaw: trimmed };
            }
            return s;
        }));
        if (activeTab() === oldName) setActiveTab(trimmed);
    };

    const handleSort = (field: 'category' | 'name' | 'value') => {
        if (sortBy() === field) {
            setSortDirection(sortDirection() === 'asc' ? 'desc' : 'asc');
        } else {
            setSortBy(field);
            setSortDirection('asc');
        }
    };

    const visibleStats = () => {
        let filtered = stats().filter(s => s.isActive && s.name.toLowerCase().includes(search().toLowerCase()));
        if (activeTab() !== "All") {
            filtered = filtered.filter(s => s.category === activeTab());
        }
        return [...filtered].sort((a, b) => {
            let valA: any = sortBy() === 'value' ? a.value : localizeCategory(sortBy() === 'category' ? a.category : a.name).toLowerCase();
            let valB: any = sortBy() === 'value' ? b.value : localizeCategory(sortBy() === 'category' ? b.category : b.name).toLowerCase();
            if (valA < valB) return sortDirection() === 'asc' ? -1 : 1;
            if (valA > valB) return sortDirection() === 'asc' ? 1 : -1;
            return 0;
        });
    };

    const settingsFilteredStats = () => {
        return stats().filter(s => {
            const matchSearch = s.name.toLowerCase().includes(settingsSearch().toLowerCase()) || s.id.toLowerCase().includes(settingsSearch().toLowerCase());
            const matchCat = settingsCatFilter() === "All" || s.category === settingsCatFilter();
            const matchVis = settingsVisFilter() === "All" || (settingsVisFilter() === "Visible" ? s.isActive : !s.isActive);
            return matchSearch && matchCat && matchVis;
        });
    };

    const toggleStatVisibility = (id: string) => {
        setStats(prev => prev.map(s => (s.id === id ? { ...s, isActive: !s.isActive } : s)));
    };

    const changeStatCategory = (id: string, newCat: string) => {
        const validatedCat = (!newCat || newCat.trim() === "") ? "General" : newCat;
        setStats(prev => prev.map(s => (s.id === id ? { ...s, category: validatedCat, categoryRaw: validatedCat } : s)));
    };

    const triggerMenuEvent = (action: string) => {
        if (typeof (window as any).TriggerMenuEvent === 'function') {
            (window as any).TriggerMenuEvent(JSON.stringify({ action }));
        }
    };

    return (
        <div class="skyrim-viewport">
            <div class="bg-gradient"></div>

            <div class="tracker-main-panel">
                <div class="skyrim-top-navigation">
                    <div class="nav-navigation-box">
                        <img src="/Assets/Side.svg" class="skyrim-nav-side left" alt="" />
                        <img src="/Assets/Side.svg" class="skyrim-nav-side right" alt="" />
                        <div class="nav-tabs-holder">
                            <button
                                class="skyrim-big-tab"
                                onClick={() => triggerMenuEvent("Journal")}
                            >
                                {t("ui.tab_quests", "Quests")}
                            </button>

                            <button class="skyrim-big-tab active">
                                <span class="tab-bracket">◁</span> {t("ui.tab_stats_tracker", "Stats Tracker")} <span class="tab-bracket">▷</span>
                            </button>

                            <button
                                class="skyrim-big-tab"
                                onClick={() => triggerMenuEvent("Pause")}
                            >
                                {t("ui.tab_system", "System")}
                            </button>
                        </div>
                    </div>
                </div>

                <div class="tracker-content-box">
                    <img src="/Assets/Side.svg" class="skyrim-frame-corner tl" alt="" />
                    <img src="/Assets/Side.svg" class="skyrim-frame-corner tr" alt="" />
                    <img src="/Assets/Side.svg" class="skyrim-frame-corner bl" alt="" />
                    <img src="/Assets/Side.svg" class="skyrim-frame-corner br" alt="" />

                    <div class="tracker-filter-bar">
                        <div class="skyrim-custom-dropdown-container">
                            <button
                                class="skyrim-custom-dropdown-trigger"
                                onClick={(e) => { e.stopPropagation(); setIsMainCatOpen(!isMainCatOpen()); }}
                            >
                                {localizeCategory(activeTab())}
                            </button>
                            <Show when={isMainCatOpen()}>
                                <div class="skyrim-custom-dropdown-options">
                                    <div
                                        class={`skyrim-custom-dropdown-option ${activeTab() === 'All' ? 'selected' : ''}`}
                                        onClick={() => { setActiveTab('All'); setSearch(""); setHoveredDesc(null); setIsMainCatOpen(false); }}
                                    >
                                        {t("ui.all_categories", "All Categories")}
                                    </div>
                                    <For each={categoriesList()}>
                                        {(cat) => (
                                            <div
                                                class={`skyrim-custom-dropdown-option ${activeTab() === cat ? 'selected' : ''}`}
                                                onClick={() => { setActiveTab(cat); setSearch(""); setHoveredDesc(null); setIsMainCatOpen(false); }}
                                            >
                                                {localizeCategory(cat)}
                                            </div>
                                        )}
                                    </For>
                                </div>
                            </Show>
                        </div>

                        <input
                            type="text"
                            class="skyrim-search-input main-search-input"
                            placeholder={t("ui.search_placeholder", "Filter statistics...")}
                            value={search()}
                            onInput={(e) => setSearch(e.currentTarget.value)}
                        />

                        <button class="key-button settings-btn" onClick={() => setIsSettingsOpen(true)}>
                            {t("ui.settings_btn", "⚙️")}
                        </button>
                    </div>

                    <div class="stat-table-header">
                        <span class={`stat-header-cell sortable ${sortBy() === 'category' ? 'active-sort' : ''}`} onClick={() => handleSort('category')}>
                            {t("ui.header_category", "Category")} {sortBy() === 'category' ? (sortDirection() === 'asc' ? ' ▲' : ' ▼') : ''}
                        </span>
                        <span class={`stat-header-cell sortable ${sortBy() === 'name' ? 'active-sort' : ''}`} onClick={() => handleSort('name')}>
                            {t("ui.header_name", "Statistic Name")} {sortBy() === 'name' ? (sortDirection() === 'asc' ? ' ▲' : ' ▼') : ''}
                        </span>
                        <span class={`stat-header-cell sortable text-right ${sortBy() === 'value' ? 'active-sort' : ''}`} onClick={() => handleSort('value')}>
                            {t("ui.header_value", "Value")} {sortBy() === 'value' ? (sortDirection() === 'asc' ? ' ▲' : ' ▼') : ''}
                        </span>
                    </div>

                    <div class="stat-list-container">
                        <Show when={visibleStats().length > 0} fallback={<div class="no-stats-fallback">{t("ui.no_stats", "No tracked statistics found.")}</div>}>
                            <For each={visibleStats()}>
                                {(stat) => (
                                    <div
                                        class="stat-list-item"
                                        onMouseEnter={() => { if (showTooltips()) setHoveredDesc(stat.desc); }}
                                        onMouseMove={(e) => setMousePos({ x: e.clientX, y: e.clientY })}
                                        onMouseLeave={() => setHoveredDesc(null)}
                                    >
                                        <span class="stat-cell-category">
                                            {localizeCategory(stat.category)}
                                        </span>
                                        <span class="stat-cell-name">
                                            {stat.name}
                                        </span>
                                        <span class="stat-cell-value">
                                            {stat.value % 1 !== 0 ? stat.value.toFixed(2) : stat.value.toLocaleString()}
                                        </span>
                                    </div>
                                )}
                            </For>
                        </Show>
                    </div>
                </div>
            </div>

            <Show when={showTooltips() && hoveredDesc()}>
                <Portal mount={document.body}>
                    <div class="stat-hover-tooltip" style={{
                        position: "fixed",
                        top: `${mousePos().y + 15}px`,
                        left: `${mousePos().x + 15}px`,
                        "z-index": 9999,
                        "pointer-events": "none"
                    }}>
                        {hoveredDesc()}
                    </div>
                </Portal>
            </Show>

            <Show when={isSettingsOpen()}>
                <Portal mount={document.body}>
                    <div class="sl-modal-overlay" onClick={saveAllChangesAndClose}>
                        <div class="settings-modal-box" onClick={(e) => e.stopPropagation()}>
                            <h2>{t("ui.modal_title", "TRACKED STATISTICS CONFIGURATION")}</h2>

                            <div class="settings-toggle-row">
                                <span>{t("ui.show_descriptions", "Show Descriptions on Hover:")}</span>
                                <div class="skyrim-checkbox" onClick={() => setShowTooltips(!showTooltips())}>
                                    {showTooltips() ? '■' : '□'}
                                </div>
                            </div>

                            <div class="settings-category-manager-container">
                                <h4 class="settings-section-title">{t("ui.category_manager", "CATEGORY MANAGER")}</h4>
                                <div class="category-add-row">
                                    <input
                                        type="text" class="skyrim-search-input" placeholder={t("ui.new_category_placeholder", "New category name...")}
                                        value={newCatName()} onInput={(e) => setNewCatName(e.currentTarget.value)}
                                    />
                                    <button class="sl-action-btn" onClick={addCategory}>{t("ui.add_btn", "+ Add")}</button>
                                </div>
                                <div class="category-manager-scroll-list">
                                    <For each={categoriesList()}>
                                        {(cat) => (
                                            <div class="category-manager-item">
                                                <input
                                                    type="text"
                                                    class="skyrim-search-input label-edit-input"
                                                    value={localizeCategory(cat)}
                                                    disabled={isSystemCategory(cat)}
                                                    style={{ color: isSystemCategory(cat) ? "#444" : "#fff" }}
                                                    onChange={(e) => renameCategory(cat, e.currentTarget.value)}
                                                />
                                                <Show when={!isSystemCategory(cat)}>
                                                    <button class="category-delete-inline-btn" onClick={() => deleteCategory(cat)}>
                                                        X
                                                    </button>
                                                </Show>
                                            </div>
                                        )}
                                    </For>
                                </div>
                            </div>

                            <div class="settings-filter-bar">
                                <input
                                    type="text" class="skyrim-search-input settings-main-search" placeholder={t("ui.settings_search_placeholder", "Search by name or ID...")}
                                    value={settingsSearch()} onInput={(e) => setSettingsSearch(e.currentTarget.value)}
                                />

                                <div class="skyrim-custom-dropdown-container settings-dropdown-width">
                                    <button class="skyrim-custom-dropdown-trigger" onClick={(e) => { e.stopPropagation(); setIsSettingsCatOpen(!isSettingsCatOpen()); setIsSettingsVisOpen(false); setOpenRowDropdownId(null); }}>
                                        {settingsCatFilter() === "All" ? t("ui.all_categories", "All Categories") : localizeCategory(settingsCatFilter())}
                                    </button>
                                    <Show when={isSettingsCatOpen()}>
                                        <div class="skyrim-custom-dropdown-options font-scale-fix">
                                            <div class={`skyrim-custom-dropdown-option ${settingsCatFilter() === 'All' ? 'selected' : ''}`} onClick={() => { setSettingsCatFilter("All"); setIsSettingsCatOpen(false); }}>{t("ui.all_categories", "All Categories")}</div>
                                            <For each={categoriesList()}>
                                                {(cat) => <div class={`skyrim-custom-dropdown-option ${settingsCatFilter() === cat ? 'selected' : ''}`} onClick={() => { setSettingsCatFilter(cat); setIsSettingsCatOpen(false); }}>{localizeCategory(cat)}</div>}
                                            </For>
                                        </div>
                                    </Show>
                                </div>

                                <div class="skyrim-custom-dropdown-container settings-dropdown-width">
                                    <button class="skyrim-custom-dropdown-trigger" onClick={(e) => { e.stopPropagation(); setIsSettingsVisOpen(!isSettingsVisOpen()); setIsSettingsCatOpen(false); setOpenRowDropdownId(null); }}>
                                        {settingsVisFilter() === "All" ? t("ui.all_visibility", "All Visibility") : settingsVisFilter() === "Visible" ? t("ui.visible_only", "Visible Only") : t("ui.hidden_only", "Hidden Only")}
                                    </button>
                                    <Show when={isSettingsVisOpen()}>
                                        <div class="skyrim-custom-dropdown-options font-scale-fix">
                                            <div class={`skyrim-custom-dropdown-option ${settingsVisFilter() === 'All' ? 'selected' : ''}`} onClick={() => { setSettingsVisFilter("All"); setIsSettingsVisOpen(false); }}>{t("ui.all_visibility", "All Visibility")}</div>
                                            <div class={`skyrim-custom-dropdown-option ${settingsVisFilter() === 'Visible' ? 'selected' : ''}`} onClick={() => { setSettingsVisFilter("Visible"); setIsSettingsVisOpen(false); }}>{t("ui.visible_only", "Visible Only")}</div>
                                            <div class={`skyrim-custom-dropdown-option ${settingsVisFilter() === 'Hidden' ? 'selected' : ''}`} onClick={() => { setSettingsVisFilter("Hidden"); setIsSettingsVisOpen(false); }}>{t("ui.hidden_only", "Hidden Only")}</div>
                                        </div>
                                    </Show>
                                </div>
                            </div>

                            <div class="settings-stat-list">
                                <For each={settingsFilteredStats()}>
                                    {(stat) => (
                                        <div class="settings-stat-row" style={{ position: "relative", "z-index": openRowDropdownId() === stat.id ? "100" : "auto" }}>
                                            <div class="settings-stat-info-block">
                                                <span class="settings-stat-name-label">{stat.name}</span>
                                                <span class="settings-stat-id-label">ID: {stat.id} {stat.isCustomRule ? "• Custom" : "• Vanilla"}</span>
                                            </div>

                                            <div class="settings-stat-actions-block">
                                                <div class="skyrim-custom-dropdown-container settings-row-dropdown-width">
                                                    <button
                                                        class="skyrim-custom-dropdown-trigger"
                                                        onClick={(e) => {
                                                            e.stopPropagation();
                                                            setOpenRowDropdownId(openRowDropdownId() === stat.id ? null : stat.id);
                                                            setIsSettingsCatOpen(false);
                                                            setIsSettingsVisOpen(false);
                                                        }}
                                                    >
                                                        {localizeCategory(stat.categoryRaw || stat.category)}
                                                    </button>

                                                    <Show when={openRowDropdownId() === stat.id}>
                                                        <div class="skyrim-custom-dropdown-options font-scale-fix">
                                                            <Show when={stat.categoryRaw && !categoriesList().includes(stat.categoryRaw)}>
                                                                <div class="skyrim-custom-dropdown-option selected" onClick={() => { changeStatCategory(stat.id, stat.categoryRaw!); setOpenRowDropdownId(null); }}>
                                                                    {localizeCategory(stat.categoryRaw!)}
                                                                </div>
                                                            </Show>
                                                            <For each={categoriesList()}>
                                                                {(cat) => (
                                                                    <div
                                                                        class={`skyrim-custom-dropdown-option ${(stat.categoryRaw || stat.category) === cat ? 'selected' : ''}`}
                                                                        onClick={() => { changeStatCategory(stat.id, cat); setOpenRowDropdownId(null); }}
                                                                    >
                                                                        {localizeCategory(cat)}
                                                                    </div>
                                                                )}
                                                            </For>
                                                        </div>
                                                    </Show>
                                                </div>

                                                <button
                                                    onClick={() => toggleStatVisibility(stat.id)}
                                                    class="visibility-toggle-btn"
                                                    style={{ background: stat.isActive ? "#166534" : "#7f1d1d" }}
                                                >
                                                    {stat.isActive ? t("ui.visible", "VISIBLE") : t("ui.hidden", "HIDDEN")}
                                                </button>
                                            </div>
                                        </div>
                                    )}
                                </For>
                            </div>

                            <div class="settings-footer-actions">
                                <button class="sl-action-btn footer-done-btn" onClick={saveAllChangesAndClose}>
                                    {t("ui.done_btn", "Done")}
                                </button>
                            </div>
                        </div>
                    </div>
                </Portal>
            </Show>
        </div>
    );
}

export default App;