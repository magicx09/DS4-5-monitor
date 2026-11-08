// A destructive hold belongs to the exact page and row where it started.
#include "settings_menu.h"
#include "test.h"

using namespace settings_menu;

TEST(settings_hold_does_not_transfer_to_another_action) {
    Nav n;
    n.in_group = true;
    n.group = 5;
    n.step(8, kTriangle, 100);
    CHECK_EQ(n.step(4, kTriangle, kHoldUs + 100).action, kNone);
    CHECK_EQ(n.row, 1);
    CHECK_EQ(n.step(8, kTriangle, 2 * kHoldUs + 100).action, kNone);
    CHECK_EQ(n.step(8, 0, 2 * kHoldUs + 101).action, kNone);
    n.step(8, kTriangle, 3 * kHoldUs);
    CHECK_EQ(n.step(8, kTriangle, 4 * kHoldUs).action, kWipe);
}

TEST(settings_hold_cannot_be_carried_in_from_the_menu) {
    Nav n;
    n.group = 5;
    n.step(8, kTriangle, 10);
    CHECK_EQ(n.step(8, kTriangle | kCross, kHoldUs + 10).action, kNone);
    CHECK_EQ(n.step(8, kTriangle, 2 * kHoldUs + 10).action, kNone);
    CHECK_EQ(n.step(8, 0, 2 * kHoldUs + 11).action, kNone);
}

TEST(settings_hold_cannot_turn_into_save_after_back) {
    Nav n;
    n.in_group = true;
    n.group = 5;
    n.step(8, kTriangle, 10);
    CHECK_EQ(n.step(8, kTriangle | kCircle, kHoldUs / 2).action, kNone);
    CHECK_EQ(n.step(8, 0, kHoldUs).action, kNone);
    CHECK(!n.in_group);
    n.step(8, kTriangle, 3 * kHoldUs);
    CHECK_EQ(n.step(8, 0, 3 * kHoldUs + 1).action, kSave);
}

TEST(settings_hold_simultaneous_navigation_requires_fresh_press) {
    Nav n;
    n.in_group = true;
    n.group = 5;
    n.step(4, kTriangle, 0);
    CHECK_EQ(n.step(8, kTriangle, kHoldUs).action, kNone);
    n.step(8, 0, kHoldUs + 1);
    n.step(8, kTriangle, kHoldUs + 2);
    CHECK_EQ(n.step(8, kTriangle, 2 * kHoldUs + 2).action, kWipe);
}

TEST(settings_hold_release_with_back_does_not_save) {
    Nav n;
    n.in_group = true;
    n.group = 5;
    n.step(8, kTriangle, 10);
    CHECK_EQ(n.step(8, kCircle, kHoldUs / 2).action, kNone);
    CHECK(!n.in_group);
}
