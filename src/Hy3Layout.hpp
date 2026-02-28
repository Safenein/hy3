#pragma once

#include <hyprland/src/desktop/DesktopTypes.hpp>
class Hy3Layout;

enum class GroupEphemeralityOption {
	Ephemeral,
	Standard,
	ForceEphemeral,
};

#include <list>
#include <set>
#include <expected>

#include <hyprland/src/layout/algorithm/TiledAlgorithm.hpp>
#include <hyprland/src/layout/space/Space.hpp>
#include <hyprland/src/layout/target/Target.hpp>

enum class ShiftDirection {
	Left,
	Up,
	Down,
	Right,
};
inline static constexpr char getShiftDirectionChar(ShiftDirection direction) {
	return direction == ShiftDirection::Left ? 'l'
	     : direction == ShiftDirection::Up   ? 'u'
	     : direction == ShiftDirection::Down ? 'd'
	                                         : 'r';
}

enum class Axis { None, Horizontal, Vertical };

#include "Hy3Node.hpp"
#include "TabGroup.hpp"

enum class FocusShift {
	Top,
	Bottom,
	Raise,
	Lower,
	Tab,
	TabNode,
};

enum class TabFocus {
	MouseLocation,
	Left,
	Right,
	Index,
};

enum class TabFocusMousePriority {
	Ignore,
	Prioritize,
	Require,
};

enum class TabLockMode {
	Lock,
	Unlock,
	Toggle,
};

enum class SetSwallowOption {
	NoSwallow,
	Swallow,
	Toggle,
};

enum class ExpandOption {
	Expand,
	Shrink,
	Base,
	Maximize,
	Fullscreen,
};

enum class ExpandFullscreenOption {
	MaximizeOnly,
	MaximizeIntermediate,
	MaximizeAsFullscreen,
};

std::pair<PHLWORKSPACE, Hy3Layout*> workspace_for_action(bool allow_fullscreen = false);

class Hy3Layout: public Layout::ITiledAlgorithm {
public:
	Hy3Layout();
	~Hy3Layout();

	// ITiledAlgorithm / IModeAlgorithm overrides
	void newTarget(SP<Layout::ITarget> target) override;
	void movedTarget(SP<Layout::ITarget> target, std::optional<Vector2D> focalPoint = std::nullopt) override;
	void removeTarget(SP<Layout::ITarget> target) override;
	void resizeTarget(const Vector2D& delta, SP<Layout::ITarget> target, Layout::eRectCorner corner = Layout::CORNER_NONE) override;
	void recalculate() override;
	void swapTargets(SP<Layout::ITarget> a, SP<Layout::ITarget> b) override;
	void moveTargetInDirection(SP<Layout::ITarget> t, Math::eDirection dir, bool silent) override;
	SP<Layout::ITarget> getNextCandidate(SP<Layout::ITarget> old) override;
	std::expected<void, std::string> layoutMsg(const std::string_view& sv) override;
	std::optional<Vector2D> predictSizeForNewTarget() override;

	// Workspace/space helpers
	PHLWORKSPACE getWorkspace();
	CBox getWorkArea();

	void insertNode(Hy3Node& node);
	void makeGroupOnWorkspace(Hy3GroupLayout, GroupEphemeralityOption, bool toggle);
	void makeOppositeGroupOnWorkspace(GroupEphemeralityOption);
	void changeGroupOnWorkspace(Hy3GroupLayout);
	void untabGroupOnWorkspace();
	void toggleTabGroupOnWorkspace();
	void changeGroupToOppositeOnWorkspace();
	void changeGroupEphemeralityOnWorkspace(bool ephemeral);
	void makeGroupOn(Hy3Node*, Hy3GroupLayout, GroupEphemeralityOption);
	void makeOppositeGroupOn(Hy3Node*, GroupEphemeralityOption);
	void changeGroupOn(Hy3Node&, Hy3GroupLayout);
	void untabGroupOn(Hy3Node&);
	void toggleTabGroupOn(Hy3Node&);
	void changeGroupToOppositeOn(Hy3Node&);
	void changeGroupEphemeralityOn(Hy3Node&, bool ephemeral);
	void shiftNode(Hy3Node&, ShiftDirection, bool once, bool visible);
	void shiftWindow(ShiftDirection, bool once, bool visible);
	void shiftFocus(ShiftDirection, bool visible, bool warp);
	void toggleFocusLayer(bool warp);
	bool shiftMonitor(Hy3Node&, ShiftDirection, bool follow);
	Hy3Node* focusMonitor(ShiftDirection);

	void warpCursor();
	void moveNodeToWorkspace(std::string wsname, bool follow, bool warp);
	void changeFocus(FocusShift);
	void focusTab(TabFocus target, TabFocusMousePriority, bool wrap_scroll, int index);
	void setNodeSwallow(SetSwallowOption);
	void killFocusedNode();
	void expand(ExpandOption, ExpandFullscreenOption);
	void setTabLock(TabLockMode);
	void equalize(bool recursive = false);
	static void warpCursorToBox(const Vector2D& pos, const Vector2D& size);
	static void warpCursorWithFocus(const Vector2D& pos, bool force = false);

	bool shouldRenderSelected(const Desktop::View::CWindow*);
	PHLWINDOW findTiledWindowCandidate(const Desktop::View::CWindow* from);
	PHLWINDOW findFloatingWindowCandidate(const Desktop::View::CWindow* from);

	Hy3Node* getWorkspaceRootGroup();
	Hy3Node* getWorkspaceFocusedNode(bool ignore_group_focus = false, bool stop_at_expanded = false);

	Hy3Node* getNodeFromWindow(const Desktop::View::CWindow*);

	std::list<Hy3Node> nodes;
	std::list<Hy3TabGroup> tab_groups;

private:
	void applyNodeDataToWindow(Hy3Node*, bool no_animation = false);

	// if shift is true, shift the window in the given direction, returning
	// nullptr, if shift is false, return the window in the given direction or
	// nullptr. if once is true, only one group will be broken out of / into
	Hy3Node* shiftOrGetFocus(Hy3Node*, ShiftDirection, bool shift, bool once, bool visible);

	void updateAutotileWorkspaces();
	bool shouldAutotileWorkspace();
	void resizeNode(Hy3Node*, Vector2D, ShiftDirection resize_edge_x, ShiftDirection resize_edge_y);

	struct {
		std::string raw_workspaces;
		bool workspace_blacklist;
		std::set<int> workspaces;
	} autotile;

	friend struct Hy3Node;
};

Hy3Node* findTabBarAt(Hy3Node& node, Vector2D pos, Hy3Node** focused_node);
