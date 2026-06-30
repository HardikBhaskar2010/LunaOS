/*
 * gui_test.c — Unit tests for LunaGUI
 */

#include "unity.h"
#include "../../../src/luna-gui/include/lunagui.h"
#include "../../../src/luna-gui/include/widget_private.h"

void setUp(void) {}
void tearDown(void) {}

void test_widget_creation(void);
void test_widget_creation(void) {
    lgui_widget_t *label = lgui_label_create("Test Label");
    TEST_ASSERT_NOT_NULL(label);
    TEST_ASSERT_EQUAL_INT(1, label->type); /* 1 = Label */
    TEST_ASSERT_EQUAL_STRING("Test Label", label->text);
    
    lgui_widget_t *box = lgui_vbox_create();
    TEST_ASSERT_NOT_NULL(box);
    TEST_ASSERT_EQUAL_INT(3, box->type); /* 3 = VBox */
    TEST_ASSERT_EQUAL_INT(0, box->child_count);
    
    lgui_box_add_child(box, label);
    TEST_ASSERT_EQUAL_INT(1, box->child_count);
    TEST_ASSERT_EQUAL_PTR(label, box->children[0]);
    
    lgui_widget_destroy(box);
}

void test_canvas_widget(void);
void test_canvas_widget(void) {
    lgui_widget_t *canvas_w = lgui_canvas_widget_create();
    TEST_ASSERT_NOT_NULL(canvas_w);
    TEST_ASSERT_EQUAL_INT(6, canvas_w->type); /* 6 = Canvas */
    
    lgui_widget_destroy(canvas_w);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_widget_creation);
    RUN_TEST(test_canvas_widget);
    return UNITY_END();
}
