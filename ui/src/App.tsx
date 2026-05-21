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
}

// Dicionário base fixo para organizar os itens Vanilla nativos
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
    const [activeTab, setActiveTab] = createSignal<string>('General');
    const [search, setSearch] = createSignal("");
    // Lista de Categorias Dinâmicas
    const [categoriesList, setCategoriesList] = createSignal<string[]>(["General", "Combat", "Magic", "Crime", "Bounties", "Crafting", "Shouts", "Quests", "DLC"]);
    const [newCatName, setNewCatName] = createSignal("");
    const [isMainCatOpen, setIsMainCatOpen] = createSignal(false);
    // Modais e Filtros internos da Janela de Edição
    const [isSettingsOpen, setIsSettingsOpen] = createSignal(false);
    const [settingsSearch, setSettingsSearch] = createSignal("");
    const [settingsCatFilter, setSettingsCatFilter] = createSignal("All");
    const [settingsVisFilter, setSettingsVisFilter] = createSignal("All");

    // Sistema de Tooltip dinâmico baseado em posição
    const [showTooltips, setShowTooltips] = createSignal(true);
    const [hoveredDesc, setHoveredDesc] = createSignal<string | null>(null);
    const [mousePos, setMousePos] = createSignal({ x: 0, y: 0 });

    const updateStatSetting = (id: string, isActive: boolean, category: string) => {
        if (typeof (window as any).UpdateStatUISettings === 'function') {
            (window as any).UpdateStatUISettings(JSON.stringify({ id, isActive, category }));
        }
    };

    onMount(() => {
        const handleStatsReceived = (e: any) => {
            const incoming: TrackedStat[] = e.detail;

            const processed = incoming.map(stat => {
                if (!stat.isCustomRule && defaultVanillaCategories[stat.id]) {
                    stat.category = defaultVanillaCategories[stat.id];
                }
                return stat;
            });

            // Une as categorias fixas com quaisquer categorias criadas dinamicamente
            const runtimeCats = new Set([...categoriesList(), ...processed.map(s => s.category)]);
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

    const addCategory = () => {
        const name = newCatName().trim();
        if (name && !categoriesList().includes(name)) {
            setCategoriesList([...categoriesList(), name]);
            setNewCatName("");
        }
    };

    const deleteCategory = (catToDelete: string) => {
        if (catToDelete === "General") return;
        setCategoriesList(categoriesList().filter(c => c !== catToDelete));
        setStats(prev => prev.map(s => {
            if (s.category === catToDelete) {
                updateStatSetting(s.id, s.isActive, "General");
                return { ...s, category: "General" };
            }
            return s;
        }));
        if (activeTab() === catToDelete) setActiveTab("General");
    };

    const renameCategory = (oldName: string, newName: string) => {
        const trimmed = newName.trim();
        if (!trimmed || oldName === "General" || categoriesList().includes(trimmed)) return;
        setCategoriesList(categoriesList().map(c => c === oldName ? trimmed : c));
        setStats(prev => prev.map(s => {
            if (s.category === oldName) {
                updateStatSetting(s.id, s.isActive, trimmed);
                return { ...s, category: trimmed };
            }
            return s;
        }));
        if (activeTab() === oldName) setActiveTab(trimmed);
    };

    const visibleStats = () => {
        return stats().filter(s =>
            s.isActive &&
            s.category === activeTab() &&
            s.name.toLowerCase().includes(search().toLowerCase())
        );
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
        setStats(prev => prev.map(s => {
            if (s.id === id) {
                const newState = !s.isActive;
                updateStatSetting(id, newState, s.category);
                return { ...s, isActive: newState };
            }
            return s;
        }));
    };

    const changeStatCategory = (id: string, newCat: string) => {
        setStats(prev => prev.map(s => {
            if (s.id === id) {
                updateStatSetting(id, s.isActive, newCat);
                return { ...s, category: newCat };
            }
            return s;
        }));
    };

    return (
        <div class="skyrim-viewport">
            <div class="bg-gradient"></div>

            {/* PAINEL PRINCIPAL DE EXIBIÇÃO */}
            <div class="tracker-main-panel">

                {/* BARRA DE FILTRO UNIFICADA NA MESMA LINHA */}
                <div class="tracker-filter-bar">
                    {/* Filtro por Categoria via Dropdown Customizado Div/Button */}
                    <div class="skyrim-custom-dropdown-container">
                        <button
                            class="skyrim-custom-dropdown-trigger"
                            onClick={(e) => { e.stopPropagation(); setIsMainCatOpen(!isMainCatOpen()); }}
                        >
                            {activeTab()}
                        </button>
                        <Show when={isMainCatOpen()}>
                            <div class="skyrim-custom-dropdown-options">
                                <For each={categoriesList()}>
                                    {(cat) => (
                                        <div
                                            class={`skyrim-custom-dropdown-option ${activeTab() === cat ? 'selected' : ''}`}
                                            onClick={() => { setActiveTab(cat); setSearch(""); setHoveredDesc(null); setIsMainCatOpen(false); }}
                                        >
                                            {cat}
                                        </div>
                                    )}
                                </For>
                            </div>
                        </Show>
                    </div>

                    {/* Filtro por Texto Customizado */}
                    <input
                        type="text"
                        class="skyrim-search-input main-search-input"
                        placeholder="Filter stats by name..."
                        value={search()}
                        onInput={(e) => setSearch(e.currentTarget.value)}
                    />

                    {/* Botão de Settings */}
                    <button class="key-button settings-btn" onClick={() => setIsSettingsOpen(true)}>
                        ⚙️ SETTINGS
                    </button>
                </div>

                {/* Lista de Atributos */}
                <div class="stat-list-container">
                    <Show when={visibleStats().length > 0} fallback={<div class="no-stats-fallback">No tracked statistics in this category.</div>}>
                        <For each={visibleStats()}>
                            {(stat) => (
                                <div
                                    class="stat-list-item"
                                    onMouseEnter={(e) => { if (showTooltips()) setHoveredDesc(stat.desc); }}
                                    onMouseMove={(e) => setMousePos({ x: e.clientX, y: e.clientY })}
                                    onMouseLeave={() => setHoveredDesc(null)}
                                >
                                    <span class="stat-name">{stat.name}</span>
                                    <span class="stat-value">
                                        {stat.value % 1 !== 0 ? stat.value.toFixed(2) : stat.value.toLocaleString()}
                                    </span>
                                </div>
                            )}
                        </For>
                    </Show>
                </div>
            </div>

            {/* TOOLTIP DINÂMICO QUE SEGUE O MOUSE */}
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

            {/* JANELA DE CONFIGURAÇÃO INTERNA */}
            <Show when={isSettingsOpen()}>
                <Portal mount={document.body}>
                    <div class="sl-modal-overlay" onClick={() => setIsSettingsOpen(false)}>
                        <div class="settings-modal-box" onClick={(e) => e.stopPropagation()}>
                            <h2>TRACKED STATS CONFIGURATION</h2>

                            <div style={{ display: "flex", "align-items": "center", "gap": "10px", "margin-bottom": "15px", "background": "#1e1e1e", padding: "12px", border: "1px solid var(--skyrim-border)" }}>
                                <span style={{ color: "#fff", "font-size": "16px" }}>Show Stat Descriptions on Hover:</span>
                                <div class="skyrim-checkbox" onClick={() => setShowTooltips(!showTooltips())}>
                                    {showTooltips() ? '■' : '□'}
                                </div>
                            </div>

                            {/* Gerenciador de Categorias Dinâmicas */}
                            <div style={{ "background": "#121212", padding: "15px", "margin-bottom": "15px", border: "1px solid #383838" }}>
                                <h4 style={{ margin: "0 0 10px 0", color: "var(--skyrim-highlight)" }}>CATEGORY MANAGER</h4>
                                <div style={{ display: "flex", gap: "10px", "margin-bottom": "10px" }}>
                                    <input
                                        type="text" class="skyrim-search-input" placeholder="New category name..."
                                        value={newCatName()} onInput={(e) => setNewCatName(e.currentTarget.value)}
                                        style={{ flex: 1 }}
                                    />
                                    <button class="sl-action-btn" style={{ margin: 0, padding: "5px 20px", "font-size": "14px", height: "37px" }} onClick={addCategory}>+ Add</button>
                                </div>
                                <div style={{ display: "grid", "grid-template-columns": "repeat(auto-fill, minmax(260px, 1fr))", gap: "8px", "max-height": "100px", "overflow-y": "auto" }}>
                                    <For each={categoriesList()}>
                                        {(cat) => (
                                            <div style={{ display: "flex", "align-items": "center", "justify-content": "space-between", background: "#1e1e1e", padding: "6px 10px", border: "1px solid #2d2d2d" }}>
                                                <input
                                                    type="text"
                                                    class="skyrim-search-input"
                                                    value={cat}
                                                    disabled={cat === "General"}
                                                    style={{ border: "none", "font-size": "14px", padding: 0, color: cat === "General" ? "#555" : "#fff", background: "transparent" }}
                                                    onChange={(e) => renameCategory(cat, e.currentTarget.value)}
                                                />
                                                <Show when={cat !== "General"}>
                                                    <button
                                                        style={{ background: "#7f1d1d", color: "white", border: "none", cursor: "pointer", padding: "4px 10px", "font-size": "11px", "font-weight": "bold", "border-radius": "2px" }}
                                                        onClick={() => deleteCategory(cat)}
                                                    >
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
                                    type="text" class="skyrim-search-input" placeholder="Search by name or ID..."
                                    style={{ flex: 1, margin: 0 }}
                                    value={settingsSearch()} onInput={(e) => setSettingsSearch(e.currentTarget.value)}
                                />
                                <div class="skyrim-select-wrapper">
                                    <select class="skyrim-dropdown" value={settingsCatFilter()} onChange={(e) => setSettingsCatFilter(e.currentTarget.value)}>
                                        <option value="All">All Categories</option>
                                        <For each={categoriesList()}>{(cat) => <option value={cat}>{cat}</option>}</For>
                                    </select>
                                </div>
                                <div class="skyrim-select-wrapper">
                                    <select class="skyrim-dropdown" value={settingsVisFilter()} onChange={(e) => setSettingsVisFilter(e.currentTarget.value)}>
                                        <option value="All">All Visibility</option>
                                        <option value="Visible">Visible Only</option>
                                        <option value="Hidden">Hidden Only</option>
                                    </select>
                                </div>
                            </div>

                            <div class="settings-stat-list">
                                <For each={settingsFilteredStats()}>
                                    {(stat) => (
                                        <div class="settings-stat-row">
                                            <div style={{ display: "flex", "flex-direction": "column", flex: 1 }}>
                                                <span class="settings-stat-name-label">{stat.name}</span>
                                                <span class="settings-stat-id-label">ID: {stat.id} {stat.isCustomRule ? "• Custom" : "• Vanilla"}</span>
                                            </div>

                                            <div style={{ display: "flex", gap: "15px", "align-items": "center" }}>
                                                <div class="skyrim-select-wrapper">
                                                    <select
                                                        class="skyrim-dropdown"
                                                        style={{ width: "150px" }}
                                                        value={stat.category}
                                                        onChange={(e) => changeStatCategory(stat.id, e.currentTarget.value)}
                                                    >
                                                        <For each={categoriesList()}>
                                                            {(cat) => <option value={cat}>{cat}</option>}
                                                        </For>
                                                    </select>
                                                </div>

                                                <button
                                                    onClick={() => toggleStatVisibility(stat.id)}
                                                    class="visibility-toggle-btn"
                                                    style={{ background: stat.isActive ? "#166534" : "#7f1d1d" }}
                                                >
                                                    {stat.isActive ? "VISIBLE" : "HIDDEN"}
                                                </button>
                                            </div>
                                        </div>
                                    )}
                                </For>
                            </div>

                            <div style={{ display: "flex", "justify-content": "flex-end", "margin-top": "20px" }}>
                                <button class="sl-action-btn" style={{ width: "auto", padding: "10px 35px", margin: 0 }} onClick={() => setIsSettingsOpen(false)}>
                                    Done
                                </button>
                            </div>
                        </div>
                    </div>
                </Portal>
            </Show>

            {/* BARRA INFERIOR LIMPA */}
            <div class="bottom-bar">
                <div class="bottom-left"></div>
                <div class="bottom-controls"></div>
            </div>
        </div>
    );
}

export default App;