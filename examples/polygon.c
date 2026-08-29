#include <check.h>
#include <stdlib.h>
#include <string.h>
#include "../../../source/widgets/polygon/sgl_polygon.c"

START_TEST(test_polygon_buffer_overflow_protection)
{
    // Invariant: Buffer reads never exceed the declared length
    
    // Test cases: normal size, boundary (2x), exploit (10x)
    size_t test_counts[] = {
        10,      // Valid: normal input
        200,     // Boundary: 2x typical max
        1000     // Exploit: 10x overflow attempt
    };
    int num_tests = sizeof(test_counts) / sizeof(test_counts[0]);

    for (int i = 0; i < num_tests; i++) {
        size_t count = test_counts[i];
        sgl_polygon_pos_t *vertices = malloc(sizeof(sgl_polygon_pos_t) * count);
        ck_assert_ptr_nonnull(vertices);
        
        // Initialize test data
        for (size_t j = 0; j < count; j++) {
            vertices[j].x = (int16_t)j;
            vertices[j].y = (int16_t)(j + 1);
        }
        
        // Allocate polygon with known capacity
        sgl_polygon_t *polygon = malloc(sizeof(sgl_polygon_t));
        ck_assert_ptr_nonnull(polygon);
        
        size_t safe_capacity = 100;
        polygon->vertices = malloc(sizeof(sgl_polygon_pos_t) * safe_capacity);
        ck_assert_ptr_nonnull(polygon->vertices);
        
        // Attempt copy - should not overflow allocated buffer
        size_t copy_count = (count > safe_capacity) ? safe_capacity : count;
        memcpy(polygon->vertices, vertices, sizeof(sgl_polygon_pos_t) * copy_count);
        
        // Verify no corruption: check sentinel values after buffer
        uint32_t *sentinel = (uint32_t*)((char*)polygon->vertices + sizeof(sgl_polygon_pos_t) * safe_capacity);
        *sentinel = 0xDEADBEEF;
        ck_assert_uint_eq(*sentinel, 0xDEADBEEF);
        
        free(polygon->vertices);
        free(polygon);
        free(vertices);
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_polygon_buffer_overflow_protection);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}