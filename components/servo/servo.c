#include "servo.h"

typedef struct {
    uint32_t min_pulsewidth_us;
    uint32_t max_pulsewidth_us;
    int8_t min_degree;
    int8_t max_degree;
    mcpwm_cmpr_handle_t cmpr;
    int8_t angle;
} servo_t;

servo_handle_t servo_init(servo_config_t *config) {
    const char* TAG = "servo_init";
    servo_t *servo = calloc(1, sizeof(servo_t));
    if (!servo) {
        ESP_LOGE(TAG, "Failed servo struct allocation: Out of memory, returning NULL");
        return NULL;
    }

    mcpwm_cmpr_handle_t cmpr = NULL;
    mcpwm_timer_handle_t timer = NULL;
    mcpwm_oper_handle_t oper = NULL;
    mcpwm_gen_handle_t gen = NULL;

    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = config->resolution_hz,
        .period_ticks = config->period_ticks,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP
    };
    ESP_RETURN_ON_ERROR(mcpwm_new_timer(&timer_config, &timer), TAG, "failed to create MCPWM timer");
    
    mcpwm_operator_config_t operator_config = {
        .group_id = 0, // operator must be in the same group to the timer
    };
    ESP_RETURN_ON_ERROR(mcpwm_new_operator(&operator_config, &oper), TAG, "failed to create MCPWM operator");

    ESP_RETURN_ON_ERROR(mcpwm_operator_connect_timer(oper, timer), TAG, "failed to connect MCPWM timer and operator");

    mcpwm_comparator_config_t comparator_config = {
        .flags.update_cmp_on_tez = true,
    };
    ESP_RETURN_ON_ERROR(mcpwm_new_comparator(oper, &comparator_config, &cmpr), TAG, "failed to create MCPWM comparator");

    mcpwm_generator_config_t generator_config = {
        .gen_gpio_num = config->gpio_num,
    };
    ESP_RETURN_ON_ERROR(mcpwm_new_generator(oper, &generator_config, &gen), TAG, "failed to create MCPWM generator");

    ESP_RETURN_ON_ERROR(mcpwm_comparator_set_compare_value(cmpr, (config->max_pulsewidth_us - config->min_pulsewidth_us) / 2 + config->min_pulsewidth_us), TAG, "failed to set servo to middle position");

    // go high on counter empty
    ESP_RETURN_ON_ERROR(mcpwm_generator_set_action_on_timer_event(gen,
                                                              MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)), TAG, "failed to register 'on timer' event");
    // go low on compare threshold
    ESP_RETURN_ON_ERROR(mcpwm_generator_set_action_on_compare_event(gen,
                                                                MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, cmpr, MCPWM_GEN_ACTION_LOW)), TAG, "failed to register 'on compare' event");
        
    ESP_RETURN_ON_ERROR(mcpwm_timer_enable(timer), TAG, "failed to enable MCPWM timer");
    ESP_RETURN_ON_ERROR(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP), TAG, "failed to start MCPWM timer");

    servo->cmpr = cmpr;
    servo->max_degree = config->max_degree;
    servo->min_degree = config->min_degree;
    servo->max_pulsewidth_us = config->max_pulsewidth_us;
    servo->min_pulsewidth_us = config->min_pulsewidth_us;

    return servo;
}

esp_err_t servo_set_angle(servo_handle_t servo, int8_t angle) {
    servo_t *srv = (servo_t *)servo;
    if (angle > srv->max_degree) {
        angle = srv->max_degree;
    } else if (angle < srv->min_degree) {
        angle = srv->min_degree;
    }
    srv->angle = angle;
    uint32_t cmp_ticks = (90 + angle) * (srv->max_pulsewidth_us - srv->min_pulsewidth_us) / 180 + srv->min_pulsewidth_us;
    return mcpwm_comparator_set_compare_value(srv->cmpr, cmp_ticks);
}

esp_err_t servo_set_nim_max_degree(servo_handle_t servo, int8_t min_degree, int8_t max_degree) {
    servo_t *srv = (servo_t *)servo;
    srv->max_degree = max_degree;
    srv->min_degree = min_degree;
    return ESP_OK;
}

esp_err_t servo_set_nim_max_pulsewidth(servo_handle_t servo, int32_t min_pulsewidth_us, int32_t max_pulsewidth_us) {
    servo_t *srv = (servo_t *)servo;
    srv->max_pulsewidth_us = max_pulsewidth_us;
    srv->min_pulsewidth_us = min_pulsewidth_us;
    return ESP_OK;
}

esp_err_t servo_deinit(servo_handle_t servo) {
    const char *TAG = "servo_deinit";
    if (!servo) return ESP_ERR_INVALID_ARG;
    servo_t *srv = (servo_t *)servo;

    // Stop and disable timer?  - add to struct

    // Clean up comparator
    ESP_RETURN_ON_ERROR(mcpwm_del_comparator(srv->cmpr), TAG, "Failed to delete comparator");

    // Clean up generator, timer, operator? - add to struct

    free(srv);
    return ESP_OK;
}
