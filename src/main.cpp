#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigDataValues.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/version.h>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/managers/SeatManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/desktop/view/WLSurface.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/devices/IPointer.hpp>
#include <hyprutils/signal/Signal.hpp>
#include <hyprlang.hpp>

#include "dispatchers.hpp"
#include "globals.hpp"

using Hyprutils::Signal::CHyprSignalListener;
using namespace Desktop::View;

// Storage for event hook listeners (released on plugin exit)
static std::vector<CHyprSignalListener> g_listeners;

static void renderHook(eRenderStage stage) {
	static bool rendering_normally = false;
	static std::vector<Hy3TabGroup*> rendered_groups;

	switch (stage) {
	case RENDER_PRE_WINDOWS:
		rendering_normally = true;
		rendered_groups.clear();
		break;
	case RENDER_POST_WINDOW:
		if (!rendering_normally) break;

		for (auto* layout: g_Hy3Instances) {
			for (auto& entry: layout->tab_groups) {
				if (!entry.hidden && entry.target_window == g_pHyprOpenGL->m_renderData.currentWindow.lock()
				    && std::find(rendered_groups.begin(), rendered_groups.end(), &entry)
				           == rendered_groups.end())
				{
					g_pHyprRenderer->m_renderPass.add(makeUnique<Hy3TabPassElement>(&entry));
					rendered_groups.push_back(&entry);
				}
			}
		}

		break;
	case RENDER_POST_WINDOWS:
		rendering_normally = false;
		break;
	default: break;
	}
}

static void windowTitleHook(PHLWINDOW window) {
	if (!window) return;
	auto* layout = getHy3Layout(window->m_workspace);
	if (!layout) return;
	auto* node = layout->getNodeFromWindow(window.get());
	if (node == nullptr) return;
	node->updateTabBarRecursive();
}

static void windowActiveHook(PHLWINDOW window, Desktop::eFocusReason) {
	if (!window) return;
	auto* layout = getHy3Layout(window->m_workspace);
	if (!layout) return;
	auto* node = layout->getNodeFromWindow(window.get());
	if (node != nullptr) {
		node->markFocused();
		auto* root = node;
		while (root->parent != nullptr) root = root->parent;
		root->recalcSizePosRecursive();
	}
}

static void windowUrgentHook(PHLWINDOW window) {
	if (!window) return;
	auto* layout = getHy3Layout(window->m_workspace);
	if (!layout) return;
	auto* node = layout->getNodeFromWindow(window.get());
	if (node != nullptr) {
		node->updateTabBarRecursive();
	}
}

static void tickHook() {
	for (auto* layout: g_Hy3Instances) {
		auto& tab_groups = layout->tab_groups;
		auto entry = tab_groups.begin();
		while (entry != tab_groups.end()) {
			entry->tick();
			if (entry->bar.destroy) tab_groups.erase(entry++);
			else entry = std::next(entry);
		}
	}
}

static void mouseButtonHook(IPointer::SButtonEvent event, Event::SCallbackInfo& info) {
	if (event.state != 1 || event.button != 272) return;

	auto ptr_surface_resource = g_pSeatManager->m_state.pointerFocus.lock();
	if (!ptr_surface_resource) return;

	auto ptr_surface = CWLSurface::fromResource(ptr_surface_resource);
	if (!ptr_surface) return;

	// non window-parented surface focused, cant have a tab
	auto view = ptr_surface->view();
	auto* window = dynamic_cast<CWindow*>(view.get());
	if (!window || window->m_isFloating || window->isFullscreen()) return;

	auto* layout = getHy3Layout(window->m_workspace);
	if (!layout) return;

	auto* node = layout->getNodeFromWindow(window);
	if (!node) return;

	auto* root = node;
	while (root->parent) root = root->parent;

	Hy3Node* focus = nullptr;
	auto mouse_pos = g_pInputManager->getMouseCoordsInternal();
	auto* tab_node = findTabBarAt(*root, mouse_pos, &focus);
	if (!tab_node) return;

	while (focus->data.is_group() && !focus->data.as_group().group_focused
	       && focus->data.as_group().focused_child != nullptr)
		focus = focus->data.as_group().focused_child;

	focus->focus(false);
	g_pInputManager->simulateMouseMovement();
	tab_node->recalcSizePosRecursive();

	info.cancelled = true;
}

APICALL EXPORT std::string PLUGIN_API_VERSION() { return HYPRLAND_API_VERSION; }

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
	PHANDLE = handle;

#ifndef HY3_NO_VERSION_CHECK
	const std::string COMPOSITOR_HASH = __hyprland_api_get_hash();
	const std::string CLIENT_HASH = __hyprland_api_get_client_hash();

	if (COMPOSITOR_HASH != CLIENT_HASH) {
		HyprlandAPI::addNotification(
		    PHANDLE,
		    "[hy3] hy3 was compiled for a different version of hyprland; refusing to load.",
		    CHyprColor {1.0, 0.2, 0.2, 1.0},
		    10000
		);

		throw std::runtime_error("[hy3] target hyprland version mismatch");
	}
#endif

#define CONF(NAME, TYPE, VALUE)                                                                    \
	HyprlandAPI::addConfigValue(PHANDLE, "plugin:hy3:" NAME, Hyprlang::CConfigValue((TYPE) VALUE))

	using Hyprlang::FLOAT;
	using Hyprlang::INT;
	using Hyprlang::STRING;

	// general
	CONF("no_gaps_when_only", INT, 0);
	CONF("node_collapse_policy", INT, 2);
	CONF("group_inset", INT, 10);
	CONF("tab_first_window", INT, 0);

	// tabs
	CONF("tabs:height", INT, 22);
	CONF("tabs:padding", INT, 5);
	CONF("tabs:from_top", INT, 0);
	CONF("tabs:radius", INT, 6);
	CONF("tabs:border_width", INT, 2);
	CONF("tabs:render_text", INT, 1);
	CONF("tabs:text_center", INT, 1);
	CONF("tabs:text_font", STRING, "Sans");
	CONF("tabs:text_height", INT, 8);
	CONF("tabs:text_padding", INT, 3);
	CONF("tabs:opacity", FLOAT, 1.0);
	CONF("tabs:blur", INT, 1);
	CONF("tabs:col.active", INT, 0x4033ccff);
	CONF("tabs:col.active.border", INT, 0xee33ccff);
	CONF("tabs:col.active.text", INT, 0xffffffff);
	CONF("tabs:col.active_alt_monitor", INT, 0x40606060);
	CONF("tabs:col.active_alt_monitor.border", INT, 0xee808080);
	CONF("tabs:col.active_alt_monitor.text", INT, 0xffffffff);
	CONF("tabs:col.focused", INT, 0x40606060);
	CONF("tabs:col.focused.border", INT, 0xee808080);
	CONF("tabs:col.focused.text", INT, 0xffffffff);
	CONF("tabs:col.inactive", INT, 0x20303030);
	CONF("tabs:col.inactive.border", INT, 0xaa606060);
	CONF("tabs:col.inactive.text", INT, 0xffffffff);
	CONF("tabs:col.urgent", INT, 0x40ff2233);
	CONF("tabs:col.urgent.border", INT, 0xeeff2233);
	CONF("tabs:col.urgent.text", INT, 0xffffffff);
	CONF("tabs:col.locked", INT, 0x40909033);
	CONF("tabs:col.locked.border", INT, 0xee909033);
	CONF("tabs:col.locked.text", INT, 0xffffffff);

	// autotiling
	CONF("autotile:enable", INT, 0);
	CONF("autotile:ephemeral_groups", INT, 1);
	CONF("autotile:trigger_height", INT, 0);
	CONF("autotile:trigger_width", INT, 0);
	CONF("autotile:workspaces", STRING, "all");

#undef CONF

	HyprlandAPI::addTiledAlgo(PHANDLE, "hy3", &typeid(Hy3Layout),
	    []() -> UP<Layout::ITiledAlgorithm> { return makeUnique<Hy3Layout>(); });

	registerDispatchers();

	// Register event hooks
	g_listeners.push_back(Event::bus()->m_events.render.stage.listen(renderHook));
	g_listeners.push_back(Event::bus()->m_events.window.title.listen(windowTitleHook));
	g_listeners.push_back(Event::bus()->m_events.window.active.listen(windowActiveHook));
	g_listeners.push_back(Event::bus()->m_events.window.urgent.listen(windowUrgentHook));
	g_listeners.push_back(Event::bus()->m_events.tick.listen(tickHook));
	g_listeners.push_back(Event::bus()->m_events.input.mouse.button.listen(mouseButtonHook));

	HyprlandAPI::reloadConfig();

	return {"hy3", "i3 like layout for hyprland", "outfoxxed", "0.1"};
}

APICALL EXPORT void PLUGIN_EXIT() {
	g_listeners.clear();
	HyprlandAPI::removeAlgo(PHANDLE, "hy3");
}
