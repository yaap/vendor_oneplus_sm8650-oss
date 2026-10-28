static int kernel_grip_init_v4_5(struct kernel_grip_info *grip_info, struct device *dev)
{
	int ret = 0;
	int temp_array[10] = {0};
	struct reclining_mode_data *reclining_data = NULL;
	struct reclining_mode_data *normal_data = NULL;

	if (grip_info == NULL) {
		return -1;
	}

	reclining_data = &grip_info->reclining_data;
	normal_data = &grip_info->normal_data;

	ret = of_property_read_u32_array(dev->of_node, (char *)"prevention,long_press_corner_point_edge_percent_thd", temp_array, 8);
	if (ret) {
		grip_info->long_press_top_corner_point_edge_percent_thd = 1600;
		grip_info->long_press_bottom_corner_point_edge_percent_thd = 2000;
		grip_info->long_press_top_corner_point_edge_min_channel_thd = 1;
		grip_info->long_press_bottom_corner_point_edge_min_channel_thd = 1;
		grip_info->long_press_top_trx_corner_point_edge_percent_thd = 33;
		grip_info->long_press_bottom_trx_corner_point_edge_percent_thd = 35;
		grip_info->long_press_time_thd = 200;
		grip_info->long_press_point_edge_degree_check_support = 0;
		TPD_INFO("V4.5 corner_point_edge_percent_thd using default.\n");
	} else {
		grip_info->long_press_top_corner_point_edge_percent_thd = temp_array[0];
		grip_info->long_press_bottom_corner_point_edge_percent_thd = temp_array[1];
		grip_info->long_press_top_corner_point_edge_min_channel_thd = temp_array[2];
		grip_info->long_press_bottom_corner_point_edge_min_channel_thd = temp_array[3];
		grip_info->long_press_top_trx_corner_point_edge_percent_thd = temp_array[4];
		grip_info->long_press_bottom_trx_corner_point_edge_percent_thd = temp_array[5];
		grip_info->long_press_time_thd = temp_array[6];
		grip_info->long_press_point_edge_degree_check_support = temp_array[7];
		TPD_INFO("V4.5 corner_point_edge_percent_thd[%d][%d][%d][%d][%d][%d][%d][%d]\n", temp_array[0], temp_array[1],
				temp_array[2], temp_array[3], temp_array[4], temp_array[5], temp_array[6], temp_array[7]);
	}

	ret = of_property_read_u32_array(dev->of_node, (char *)"prevention,center_down_corner_point_edge_percent_thd", temp_array, 8);
	if (ret) {
		grip_info->center_down_top_corner_point_edge_percent_thd = 1600;
		grip_info->center_down_bottom_corner_point_edge_percent_thd = 2000;
		grip_info->center_down_top_corner_point_edge_min_channel_thd = 1;
		grip_info->center_down_bottom_corner_point_edge_min_channel_thd = 1;
		grip_info->center_down_top_trx_corner_point_edge_percent_thd = 33;
		grip_info->center_down_bottom_trx_corner_point_edge_percent_thd = 35;
		grip_info->center_down_point_edge_degree_check_support = 0;
		TPD_INFO("V4.5 corner_point_edge_percent_thd using default.\n");
	} else {
		grip_info->center_down_top_corner_point_edge_percent_thd = temp_array[0];
		grip_info->center_down_bottom_corner_point_edge_percent_thd = temp_array[1];
		grip_info->center_down_top_corner_point_edge_min_channel_thd = temp_array[2];
		grip_info->center_down_bottom_corner_point_edge_min_channel_thd = temp_array[3];
		grip_info->center_down_top_trx_corner_point_edge_percent_thd = temp_array[4];
		grip_info->center_down_bottom_trx_corner_point_edge_percent_thd = temp_array[5];
		grip_info->center_down_point_edge_degree_check_support = temp_array[7];
		TPD_INFO("V4.5 corner_point_edge_percent_thd[%d][%d][%d][%d][%d][%d][%d]\n", temp_array[0], temp_array[1],
				temp_array[2], temp_array[3], temp_array[4], temp_array[5], temp_array[7]);
	}

	ret = of_property_read_u32_array(dev->of_node, (char *)"prevention,soon_meet_corner_point_edge_percent_thd", temp_array, 8);
	if (ret) {
		grip_info->soon_meet_top_corner_point_edge_percent_thd = 1600;
		grip_info->soon_meet_bottom_corner_point_edge_percent_thd = 2000;
		grip_info->soon_meet_top_corner_point_edge_min_channel_thd = 1;
		grip_info->soon_meet_bottom_corner_point_edge_min_channel_thd = 1;
		grip_info->soon_meet_top_trx_corner_point_edge_percent_thd = 33;
		grip_info->soon_meet_bottom_trx_corner_point_edge_percent_thd = 35;
		grip_info->soon_meet_min_level_thd = 3;
		grip_info->soon_meet_point_edge_degree_check_support = 0;
		TPD_INFO("V4.5 corner_point_edge_percent_thd using default.\n");
	} else {
		grip_info->soon_meet_top_corner_point_edge_percent_thd = temp_array[0];
		grip_info->soon_meet_bottom_corner_point_edge_percent_thd = temp_array[1];
		grip_info->soon_meet_top_corner_point_edge_min_channel_thd = temp_array[2];
		grip_info->soon_meet_bottom_corner_point_edge_min_channel_thd = temp_array[3];
		grip_info->soon_meet_top_trx_corner_point_edge_percent_thd = temp_array[4];
		grip_info->soon_meet_bottom_trx_corner_point_edge_percent_thd = temp_array[5];
		grip_info->soon_meet_min_level_thd = temp_array[6];
		grip_info->soon_meet_point_edge_degree_check_support = temp_array[7];
		TPD_INFO("V4.5 corner_point_edge_percent_thd[%d][%d][%d][%d][%d][%d][%d][%d]\n", temp_array[0], temp_array[1],
				temp_array[2], temp_array[3], temp_array[4], temp_array[5], temp_array[6], temp_array[7]);
	}
	return 0;
}

static int kernel_grip_init_v4_3(struct kernel_grip_info *grip_info, struct device *dev)
{
	int ret = 0;
	int temp_array[10] = {0};
	int m_index = 0;
	struct reclining_mode_data *reclining_data = NULL;
	struct reclining_mode_data *normal_data = NULL;

	if (grip_info == NULL) {
		return -1;
	}

	reclining_data = &grip_info->reclining_data;
	normal_data = &grip_info->normal_data;

	ret = of_property_read_u32_array(dev->of_node, (char *)"prevention,corner_point_edge_percent_thd", temp_array, 5);
	if (ret) {
		grip_info->corner_point_edge_percent_thd = 1600;
		grip_info->corner_point_edge_min_channel_thd = 1;
		grip_info->top_trx_corner_point_edge_percent_thd = 33;
		grip_info->bottom_trx_corner_point_edge_percent_thd = 35;
		grip_info->point_edge_degree_check_support = 0;
		TPD_INFO("V4.3 corner_point_edge_percent_thd using default.\n");
	} else {
		grip_info->corner_point_edge_percent_thd = temp_array[0];
		grip_info->corner_point_edge_min_channel_thd = temp_array[1];
		grip_info->top_trx_corner_point_edge_percent_thd = temp_array[2];
		grip_info->bottom_trx_corner_point_edge_percent_thd = temp_array[3];
		grip_info->point_edge_degree_check_support = temp_array[4];
		TPD_INFO("V4.3 corner_point_edge_percent_thd[%d][%d][%d][%d][%d]\n", temp_array[0], temp_array[1],
				temp_array[2], temp_array[3], temp_array[4]);
	}

	ret = of_property_read_u32_array(dev->of_node,  (char *)"prevention,corner_point_edge_percent_thd2", temp_array, 5);
	if (ret) {
		grip_info->corner_point_trx_radio_thd = 30;
		grip_info->corner_point_check_frame_thd = 3;
		grip_info->corner_point_peakdelta_stable_thd = 20;
		grip_info->bottom_corner_point_edge_percent_thd = 1300;
		TPD_INFO("corner_point_edge_percent_thd using default.\n");
	} else {
		grip_info->corner_point_trx_radio_thd = temp_array[0];
		grip_info->corner_point_check_frame_thd = temp_array[1];
		grip_info->corner_point_peakdelta_stable_thd = temp_array[2];
		grip_info->bottom_corner_point_edge_percent_thd = temp_array[3];
	}

	ret = of_property_read_u32_array(dev->of_node,  (char *)"prevention,point_stability_judgment_check", temp_array, 5);
	if (ret) {
		grip_info->point_stability_press_jitter_thd = 2;
		grip_info->point_stability_er_jitter_thd = 2;
		grip_info->point_stability_er_jitter_percent_thd = 10;
		grip_info->point_stability_judgment_check_support = 0;
		TPD_INFO("corner_point_edge_percent_thd using default.\n");
	} else {
		grip_info->point_stability_press_jitter_thd = temp_array[0];
		grip_info->point_stability_er_jitter_thd = temp_array[1];
		grip_info->point_stability_er_jitter_percent_thd = temp_array[2];
		grip_info->point_stability_judgment_check_support = temp_array[4];
	}

	ret = of_property_read_u32_array(dev->of_node, (char *)"prevention,point_corner_er_min_limit_check", temp_array, 5);
	if (ret) {
		grip_info->point_corner_tx_er_min_limit_thd = 3;
		grip_info->point_corner_rx_er_min_limit_thd = 3;
		grip_info->point_corner_er_min_limit_support = 0;
		TPD_INFO("point_corner_er_min_limit_check using default.\n");
	} else {
		grip_info->point_corner_tx_er_min_limit_thd = temp_array[0];
		grip_info->point_corner_rx_er_min_limit_thd = temp_array[1];
		grip_info->point_corner_er_min_limit_support = temp_array[4];
	}

	ret = of_property_read_u32_array(dev->of_node, (char *)"prevention,one_frame_down_check_support", temp_array, 1);
	if (ret) {
		grip_info->one_frame_down_check_support = 0;
		TPD_INFO("one_frame_down_check_support using default.\n");
	} else {
		grip_info->one_frame_down_check_support = temp_array[0];
	}

	ret = of_property_read_u32_array(dev->of_node, (char *)"prevention,uniform_make_up_point_v2", temp_array, 5);
	if (ret) {
		grip_info->uniform_make_up_point_num = 10;
		grip_info->uniform_make_up_exit_point_num = 2;
		grip_info->uniform_max_make_up_time = 45;
		grip_info->uniform_make_up_last_point_percent_limit = 5;
		grip_info->uniform_make_up_point_v2_support = 0;
		TPD_INFO("point_corner_er_min_limit_check using default.\n");
	} else {
		grip_info->uniform_make_up_point_num = temp_array[0];
		grip_info->uniform_make_up_exit_point_num = temp_array[1];
		grip_info->uniform_max_make_up_time = temp_array[2];
		grip_info->uniform_make_up_last_point_percent_limit = temp_array[3];
		grip_info->uniform_make_up_point_v2_support = temp_array[4];
	}

	for (m_index = 0; m_index < TOUCH_MAX_NUM; m_index++) {
		grip_info->uniform_make_up_point_num_status[m_index] = 0;
	}
	grip_info->uniform_make_up_point_upid_status = 0;


	ret = of_property_read_u32_array(dev->of_node, (char *)"prevention,touch_point_stability_check", temp_array, 5);
	if (ret) {
		grip_info->touch_point_stability_max_time = 100;
		grip_info->middle_bottom_long_press_time_thd = 500;
		TPD_INFO("touch_point_stability_check using default.\n");
	} else {
		grip_info->touch_point_stability_max_time = temp_array[0];
		grip_info->middle_bottom_long_press_time_thd = temp_array[1];
	}
	ret = of_property_read_u32_array(dev->of_node, (char *)"prevention,middle_bottom_long_press_check", temp_array, 5);
	if (ret) {
		grip_info->middle_bottom_long_press_width_min_thd = 6;
		grip_info->middle_bottom_long_press_minor_max_thd = 140;
		grip_info->middle_bottom_long_press_match_times_thd = 10;
		grip_info->large_middle_bottom_exit_quickly_support = 0;
		TPD_INFO("middle_bottom_long_press_check using default.\n");
	} else {
		grip_info->middle_bottom_long_press_width_min_thd = temp_array[0];
		grip_info->middle_bottom_long_press_minor_max_thd = temp_array[1];
		grip_info->middle_bottom_long_press_match_times_thd = temp_array[2];
		grip_info->large_middle_bottom_exit_quickly_support = temp_array[4];
	}

	ret = of_property_read_u32_array(dev->of_node, (char *)"prevention,middle_bottom_small_press_check", temp_array, 5);
	if (ret) {
		grip_info->middle_bottom_small_press_width_max_thd = 4;
		grip_info->middle_bottom_small_press_er_min_thd = 70;
		grip_info->middle_bottom_small_press_peakdelta_max_thd = 550;
		grip_info->middle_bottom_small_press_minor_max_thd = 140;
		grip_info->middle_bottom_small_press_match_times_thd = 10;
		TPD_INFO("middle_bottom_small_press_check using default.\n");
	} else {
		grip_info->middle_bottom_small_press_width_max_thd = temp_array[0];
		grip_info->middle_bottom_small_press_er_min_thd = temp_array[1];
		grip_info->middle_bottom_small_press_peakdelta_max_thd = temp_array[2];
		grip_info->middle_bottom_small_press_minor_max_thd = temp_array[3];
		grip_info->middle_bottom_small_press_match_times_thd = temp_array[4];
	}

	ret = of_property_read_u32_array(dev->of_node, (char *)"prevention,middle_bottom_exit_dis", temp_array, 5);
	if (ret) {
		grip_info->middle_bottom_exit_x_dis_thd = 960;
		grip_info->middle_bottom_exit_y_dis_thd = 640;
		grip_info->middle_bottom_match_exit_x_dis_percent_thd = 20;
		grip_info->middle_bottom_match_exit_y_dis_thd = 640;
		TPD_INFO("middle_bottom_exit_dis using default.\n");
	} else {
		grip_info->middle_bottom_exit_x_dis_thd = temp_array[0];
		grip_info->middle_bottom_exit_y_dis_thd = temp_array[1];
		grip_info->middle_bottom_match_exit_x_dis_percent_thd = temp_array[2];
		grip_info->middle_bottom_match_exit_y_dis_thd = temp_array[3];
	}

	ret = of_property_read_u32_array(dev->of_node, (char *)"prevention,middle_bottom_yfsr_coupling_thd", temp_array, 5);
	if (ret) {
		grip_info->middle_bottom_short_exit_yfsr_coupling_thd = 90;
		grip_info->middle_bottom_long_exit_yfsr_coupling_thd = 60;
		grip_info->middle_bottom_normal_press_thd = 5;
		grip_info->middle_bottom_exit_yfsr_coupling_times_thd = 10;
		TPD_INFO("middle_bottom_yfsr_coupling_thd using default.\n");
	} else {
		grip_info->middle_bottom_short_exit_yfsr_coupling_thd = temp_array[0];
		grip_info->middle_bottom_long_exit_yfsr_coupling_thd = temp_array[1];
		grip_info->middle_bottom_normal_press_thd = temp_array[2];
		grip_info->middle_bottom_exit_yfsr_coupling_times_thd = temp_array[3];
	}

	return 0;
}

static int kernel_grip_init_v4(struct kernel_grip_info *grip_info, struct device *dev)
{
	int ret = 0;
	int temp_array[10] = {0};
	struct reclining_mode_data *reclining_data = NULL;
	struct reclining_mode_data *normal_data = NULL;

	if (grip_info == NULL) {
		return -1;
	}

	reclining_data = &grip_info->reclining_data;
	normal_data = &grip_info->normal_data;

	ret = of_property_read_u32_array(dev->of_node, (char *)"prevention,long_fingerhold_condition2", temp_array, 4);
	if (ret) {
		grip_info->long_hold_x_width = 0xffff;/*default value 0, no need to change it*/
		grip_info->long_hold_y_width = grip_info->max_y / 4;
		grip_info->finger_hold_matched_hor_support = 0;
		GRIP_TP_INFO("large long finger hold long_fingerhold_condition2 using default.\n");
	} else {
		grip_info->long_hold_x_width = temp_array[0];
		grip_info->long_hold_y_width = temp_array[1];
		grip_info->finger_hold_matched_hor_support = temp_array[2];
	}

	ret = of_property_read_u32_array(dev->of_node, (char *)"prevention,top_corner_config2", temp_array, 5);
	if (ret) {
		grip_info->top_matched_times_thd = 3;
		grip_info->top_matched_xfsr_thd = 80;
		grip_info->large_ver_top_exit_distance = 200;
		GRIP_TP_INFO("large long finger hold long_fingerhold_condition2 using default.\n");
	} else {
		grip_info->top_matched_times_thd = temp_array[0];
		grip_info->top_matched_xfsr_thd = temp_array[1];
		grip_info->large_ver_top_exit_distance = temp_array[2];
	}

	ret = of_property_read_u32_array(dev->of_node, (char *)"prevention,hor_long_top_corner_config", temp_array, 4);
	if (ret) {
		grip_info->large_hor_top_x_width = 280;
		grip_info->large_hor_top_y_height = 1000;
		GRIP_TP_INFO("large long finger hold hor_long_top_corner_config using default.\n");
	} else {
		grip_info->large_hor_top_x_width = temp_array[0];
		grip_info->large_hor_top_y_height = temp_array[1];
	}

	ret = of_property_read_u32_array(dev->of_node, (char *)"prevention,top_middle_config", temp_array, 4);
	if (ret) {
		grip_info->large_top_middle_width = grip_info->max_x;
		grip_info->large_top_middle_height = 500;
		grip_info->large_top_middle_exit_distance = 500;
		grip_info->large_bottom_middle_support = 0;
		GRIP_TP_INFO("vertical top bottom middle config using default.\n");
	} else {
		grip_info->large_top_middle_width = temp_array[0];
		grip_info->large_top_middle_height = temp_array[1];
		grip_info->large_top_middle_exit_distance = temp_array[2];
		grip_info->large_bottom_middle_support = temp_array[3];
	}
	ret = of_property_read_u32_array(dev->of_node, (char *)"prevention,corner_eliminate_point_type", temp_array, 4);
	if (ret) {
		grip_info->corner_eliminate_point_type = 0;
		grip_info->large_corner_hor_x_width = 0;
		grip_info->large_corner_hor_y_height = 0;
		grip_info->corner_eliminate_without_time = 0;
		GRIP_TP_INFO("corner_eliminate_point_type config using default.\n");
	} else {
		grip_info->corner_eliminate_point_type = temp_array[0];
		grip_info->large_corner_hor_x_width = temp_array[1];
		grip_info->large_corner_hor_y_height = temp_array[2];
		grip_info->corner_eliminate_without_time = temp_array[3];
	}
	ret = of_property_read_u32_array(dev->of_node, (char *)"prevention,long_eliminate_point_type", temp_array, 7);
	if (ret) {
		grip_info->long_eliminate_point_type = 1 << POS_CENTER_INNER;
		grip_info->large_long_x2_width = 0;
		grip_info->large_long_y2_width = 0;
		normal_data->large_long_x1_width = 0;
		grip_info->large_long_y1_width = 0;
		normal_data->large_long_debounce_ms = 300;
		grip_info->long_eliminate_point_support = false;
		GRIP_TP_INFO("long_hold_eliminate_point_type config using default.\n");
	} else {
		grip_info->long_eliminate_point_type = temp_array[0];
		grip_info->large_long_x2_width = temp_array[1];
		grip_info->large_long_y2_width = temp_array[2];
		normal_data->large_long_x1_width = temp_array[3];
		grip_info->large_long_y1_width = temp_array[4];
		normal_data->large_long_debounce_ms = temp_array[5];
		grip_info->long_eliminate_point_support = temp_array[6];
	}
	ret = of_property_read_u32_array(dev->of_node, (char *)"prevention,finger_hold_differ_size", temp_array, 4);
	if (ret) {
		grip_info->finger_hold_differ_size_support = 0;
		grip_info->finger_hold_differ_hor_support = 0;
		normal_data->finger_hold_differ_size_x = 0;
		normal_data->finger_hold_differ_size_debounce_ms = 300;
		GRIP_TP_INFO("finger_hold_differ_size config using default.\n");
	} else {
		grip_info->finger_hold_differ_size_support = temp_array[0];
		grip_info->finger_hold_differ_hor_support = temp_array[1];
		normal_data->finger_hold_differ_size_x = temp_array[2];
		normal_data->finger_hold_differ_size_debounce_ms = temp_array[3];
	}
	ret = of_property_read_u32_array(dev->of_node, (char *)"prevention,finger_hold_max_rx_exit_distance", temp_array, 3);
	if (ret) {
		normal_data->finger_hold_rx_rejec_thd = 0xff;
		normal_data->finger_hold_max_rx_narrow_witdh = grip_info->edge_swipe_narrow_witdh;
		normal_data->finger_hold_max_rx_exit_distance = grip_info->edge_swipe_exit_distance;
		GRIP_TP_INFO("finger_hold_max_rx_exit_distance config using default.\n");
	} else {
		normal_data->finger_hold_rx_rejec_thd = temp_array[0];
		normal_data->finger_hold_max_rx_narrow_witdh = temp_array[1];
		normal_data->finger_hold_max_rx_exit_distance = temp_array[2];
	}
	ret = of_property_read_u32_array(dev->of_node, (char *)"prevention,max_rx_exit_distance", temp_array, 5);
	if (ret) {
		grip_info->max_rx_matched_support = 0;
		normal_data->max_rx_rejec_thd = 0xff;
		normal_data->max_rx_stable_time_thd = 150;
		normal_data->max_rx_narrow_witdh = grip_info->edge_swipe_narrow_witdh;
		normal_data->max_rx_exit_distance = grip_info->edge_swipe_exit_distance;
		GRIP_TP_INFO("max_rx_exit_distance config using default.\n");
	} else {
		grip_info->max_rx_matched_support = temp_array[0];
		normal_data->max_rx_rejec_thd = temp_array[1];
		normal_data->max_rx_stable_time_thd = temp_array[2];
		normal_data->max_rx_narrow_witdh = temp_array[3];
		normal_data->max_rx_exit_distance = temp_array[4];
	}
	ret = of_property_read_u32_array(dev->of_node, (char *)"prevention,dynamic_finger_hold_exit_distance", temp_array, 4);
	if (ret) {
		grip_info->dynamic_finger_hold_exit_support = 0;
		normal_data->dynamic_finger_hold_narrow_witdh = grip_info->edge_swipe_narrow_witdh;
		normal_data->dynamic_finger_hold_exit_distance = grip_info->edge_swipe_exit_distance;
		normal_data->dynamic_finger_hold_size_x = 30;
		GRIP_TP_INFO("dynamic_finger_hold_exit_distance config using default.\n");
	} else {
		grip_info->dynamic_finger_hold_exit_support = temp_array[0];
		normal_data->dynamic_finger_hold_narrow_witdh = temp_array[1];
		normal_data->dynamic_finger_hold_exit_distance = temp_array[2];
		normal_data->dynamic_finger_hold_size_x = temp_array[3];
	}
	ret = of_property_read_u32_array(dev->of_node, (char *)"prevention,edge_sliding_matched_support", temp_array, 4);
	if (ret) {
		grip_info->edge_sliding_matched_support = 0;
		normal_data->edge_sliding_exit_yfsr_thd = 0xffff;
		normal_data->edge_sliding_exit_distance = grip_info->max_y / 3;
		grip_info->edge_swipe_makeup_optimization_support = 0;
		GRIP_TP_INFO("edge_sliding_matched_support config using default.\n");
	} else {
		grip_info->edge_sliding_matched_support = temp_array[0];
		normal_data->edge_sliding_exit_yfsr_thd = temp_array[1];
		normal_data->edge_sliding_exit_distance = grip_info->max_y / temp_array[2];
		grip_info->edge_swipe_makeup_optimization_support = temp_array[3];
	}

	ret = of_property_read_u32_array(dev->of_node, "prevention,large_corner_judge_condition_ime", temp_array, 5);
	if (ret) {
		grip_info->large_corner_detect_time_ms_ime = grip_info->large_corner_detect_time_ms;
		GRIP_TP_INFO("large corner judge condition using default.\n");
	} else {
		grip_info->large_corner_detect_time_ms_ime = temp_array[0];
	}

	transform_normal_para(grip_info);

	ret = of_property_read_u32_array(dev->of_node, (char *)"prevention,reclining_mode_support", temp_array, 1);
	if (ret) {
		grip_info->reclining_mode_support = 0;
		GRIP_TP_INFO("reclining_mode_support config using default.\n");
	} else {
		grip_info->reclining_mode_support = temp_array[0];
	}
	ret = of_property_read_u32_array(dev->of_node, (char *)"prevention,long_eliminate_point_type_recli", temp_array, 7);
	if (ret) {
		reclining_data->large_long_x1_width = normal_data->large_long_x1_width;
		reclining_data->large_long_debounce_ms = normal_data->large_long_debounce_ms;
		GRIP_TP_INFO("long_hold_eliminate_point_type config using default.\n");
	} else {
		reclining_data->large_long_x1_width = temp_array[3];
		reclining_data->large_long_debounce_ms = temp_array[5];
	}
	ret = of_property_read_u32_array(dev->of_node, (char *)"prevention,finger_hold_differ_size_recli", temp_array, 4);
	if (ret) {
		reclining_data->finger_hold_differ_size_x = normal_data->finger_hold_differ_size_x;
		reclining_data->finger_hold_differ_size_debounce_ms = normal_data->finger_hold_differ_size_debounce_ms;
		GRIP_TP_INFO("finger_hold_differ_size config using default.\n");
	} else {
		reclining_data->finger_hold_differ_size_x = temp_array[2];
		reclining_data->finger_hold_differ_size_debounce_ms = temp_array[3];
	}
	ret = of_property_read_u32_array(dev->of_node, (char *)"prevention,finger_hold_max_rx_exit_distance_recli", temp_array, 3);
	if (ret) {
		reclining_data->finger_hold_rx_rejec_thd = normal_data->finger_hold_rx_rejec_thd;
		reclining_data->finger_hold_max_rx_narrow_witdh = normal_data->finger_hold_max_rx_narrow_witdh;
		reclining_data->finger_hold_max_rx_exit_distance = normal_data->finger_hold_max_rx_exit_distance;
		GRIP_TP_INFO("finger_hold_max_rx_exit_distance config using default.\n");
	} else {
		reclining_data->finger_hold_rx_rejec_thd = temp_array[0];
		reclining_data->finger_hold_max_rx_narrow_witdh = temp_array[1];
		reclining_data->finger_hold_max_rx_exit_distance = temp_array[2];
	}
	ret = of_property_read_u32_array(dev->of_node, (char *)"prevention,max_rx_exit_distance_recli", temp_array, 5);
	if (ret) {
		reclining_data->max_rx_rejec_thd = normal_data->max_rx_rejec_thd;
		reclining_data->max_rx_stable_time_thd = normal_data->max_rx_stable_time_thd;
		reclining_data->max_rx_narrow_witdh = normal_data->max_rx_exit_distance;
		reclining_data->max_rx_exit_distance = normal_data->max_rx_exit_distance;
		GRIP_TP_INFO("max_rx_exit_distance config using default.\n");
	} else {
		reclining_data->max_rx_rejec_thd = temp_array[1];
		reclining_data->max_rx_stable_time_thd = temp_array[2];
		reclining_data->max_rx_narrow_witdh = temp_array[3];
		reclining_data->max_rx_exit_distance = temp_array[4];
	}
	ret = of_property_read_u32_array(dev->of_node, (char *)"prevention,dynamic_finger_hold_exit_distance_recli", temp_array, 4);
	if (ret) {
		reclining_data->dynamic_finger_hold_narrow_witdh = normal_data->dynamic_finger_hold_narrow_witdh;
		reclining_data->dynamic_finger_hold_exit_distance = normal_data->dynamic_finger_hold_narrow_witdh;
		reclining_data->dynamic_finger_hold_size_x = normal_data->dynamic_finger_hold_narrow_witdh;
		GRIP_TP_INFO("dynamic_finger_hold_exit_distance config using default.\n");
	} else {
		reclining_data->dynamic_finger_hold_narrow_witdh = temp_array[1];
		reclining_data->dynamic_finger_hold_exit_distance = temp_array[2];
		reclining_data->dynamic_finger_hold_size_x = temp_array[3];
	}
	ret = of_property_read_u32_array(dev->of_node, (char *)"prevention,edge_sliding_matched_support_recli", temp_array, 4);
	if (ret) {
		reclining_data->edge_sliding_exit_yfsr_thd = normal_data->edge_sliding_exit_yfsr_thd;
		reclining_data->edge_sliding_exit_distance = normal_data->edge_sliding_exit_distance;
		GRIP_TP_INFO("edge_sliding_matched_support config using default.\n");
	} else {
		reclining_data->edge_sliding_exit_yfsr_thd = temp_array[1];
		reclining_data->edge_sliding_exit_distance = grip_info->max_y / temp_array[2];
	}

	ret = of_property_read_u32_array(dev->of_node, "prevention,long_edge_condition_recli", temp_array, 4);
	if (ret) {
		reclining_data->long_start_coupling_thd = normal_data->long_start_coupling_thd;
		reclining_data->long_stable_coupling_thd = normal_data->long_stable_coupling_thd;
		reclining_data->long_hold_changed_thd = normal_data->long_hold_changed_thd;
		reclining_data->long_hold_maxfsr_gap = normal_data->long_hold_maxfsr_gap;
		GRIP_TP_INFO("long_edge_condition_recli using default.\n");
	} else {
		reclining_data->long_start_coupling_thd = temp_array[0];
		reclining_data->long_stable_coupling_thd = temp_array[1];
		reclining_data->long_hold_changed_thd = temp_array[2];
		reclining_data->long_hold_maxfsr_gap = temp_array[3];
	}

	return 0;
}

static int kernel_grip_init_V2(struct kernel_grip_info *grip_info, struct device *dev)
{
	int i_index, j_index, cnt = 0, ret = 0;
	int makeup_para[10] = {0}, temp_array[10] = {0};
	struct reclining_mode_data *normal_data = NULL;


	if (grip_info == NULL) {
		return -1;
	}
	normal_data = &grip_info->normal_data;

	//parameter init
	mutex_init(&grip_info->grip_mutex);
	ret = of_property_read_u32_array(dev->of_node, "touchpanel,panel-coords", temp_array, 2);
	if (ret) {
		grip_info->max_x = 1080;
		grip_info->max_y = 2340;
		GRIP_TP_INFO("panel coords using default.\n");
	} else {
		grip_info->max_x = temp_array[0];
		grip_info->max_y = temp_array[1];
	}

	grip_info->tx_rx_num_exchange_support = of_property_read_bool(dev->of_node, "tx_rx_num_exchange_support");
	ret = of_property_read_u32_array(dev->of_node, "touchpanel,tx-rx-num", temp_array, 2);
	if (ret) {
		grip_info->tx_num = 0;
		grip_info->rx_num = 0;
		GRIP_TP_INFO("panel tx rx not set.\n");
	} else {
		if (grip_info->tx_rx_num_exchange_support) {
			grip_info->tx_num = temp_array[1];
			grip_info->rx_num = temp_array[0];
		} else {
			grip_info->tx_num = temp_array[0];
			grip_info->rx_num = temp_array[1];
		}
	}

	ret = of_property_read_u32_array(dev->of_node, "prevention,grip_disable_level", temp_array, 1);
	if (ret) {
		grip_info->grip_disable_level = 0;
		GRIP_TP_INFO("grip disable level using default.\n");
	} else {
		grip_info->grip_disable_level = temp_array[0];
	}

	cnt = of_property_count_elems_of_size(dev->of_node, "prevention,makeup_cnt_weight", sizeof(uint32_t));
	if ((cnt > 0) && (cnt < 10)) {
		ret = of_property_read_u32_array(dev->of_node, "prevention,makeup_cnt_weight", makeup_para, cnt);
	} else {
		ret = -1;
	}
	if (ret || (makeup_para[0] + 1 != cnt)) {
		makeup_para[0] = 4;
		makeup_para[1] = 1;
		makeup_para[2] = 2;
		makeup_para[3] = 2;
		makeup_para[4] = 1;
		GRIP_TP_INFO("makeup cnt and weight using default.\n");
	}

	ret = of_property_read_u32_array(dev->of_node, "prevention,updelay_time_ms", temp_array, 1);
	if (ret) {
		grip_info->report_updelay_ms = 30;
		GRIP_TP_INFO("grip updelay time using default.\n");
	} else {
		grip_info->report_updelay_ms = temp_array[0];
	}

	ret = of_property_read_u32_array(dev->of_node, "prevention,large_corner_range", temp_array, 2);
	if (ret) {
		grip_info->large_corner_width = 200;
		grip_info->large_corner_height = 300;
		GRIP_TP_INFO("large corner range using default.\n");
	} else {
		grip_info->large_corner_width = temp_array[0];
		grip_info->large_corner_height = temp_array[1];
	}

	ret = of_property_read_u32_array(dev->of_node, "prevention,large_corner_judge_condition", temp_array, 5);
	if (ret) {
		grip_info->large_corner_detect_time_ms = 1000;
		grip_info->large_corner_debounce_ms = 80;
		grip_info->large_corner_exit_distance = 75;
		grip_info->xfsr_corner_exit_thd = 4;
		grip_info->yfsr_corner_exit_thd = 4;
		GRIP_TP_INFO("large corner judge condition using default.\n");
	} else {
		grip_info->large_corner_detect_time_ms = temp_array[0];
		grip_info->large_corner_debounce_ms = temp_array[1];
		grip_info->large_corner_exit_distance = temp_array[2];
		grip_info->xfsr_corner_exit_thd = temp_array[3];
		grip_info->yfsr_corner_exit_thd = temp_array[4];
	}
	grip_info->large_corner_detect_time_ms_restore = grip_info->large_corner_detect_time_ms;

	ret = of_property_read_u32_array(dev->of_node, "prevention,trx_reject_condition", temp_array, 3);
	if (ret) {
		grip_info->trx_reject_thd = 8;
		grip_info->rx_reject_thd = 5;
		grip_info->tx_reject_thd = 5;
		GRIP_TP_INFO("tx rx reject condition using default.\n");
	} else {
		grip_info->trx_reject_thd = temp_array[0];
		grip_info->rx_reject_thd = temp_array[1];
		grip_info->tx_reject_thd = temp_array[2];
	}

	ret = of_property_read_u32_array(dev->of_node, "prevention,large_judge_time_ms", temp_array, 3);
	if (ret) {
		grip_info->long_detect_time_ms = 400;
		grip_info->large_reject_debounce_time_ms = 60;
		grip_info->fsr_stable_time_thd = 30;
		GRIP_TP_INFO("large judge times using default.\n");
	} else {
		grip_info->long_detect_time_ms = temp_array[0];
		grip_info->large_reject_debounce_time_ms = temp_array[1];
		grip_info->fsr_stable_time_thd = temp_array[2];
	}

	ret = of_property_read_u32_array(dev->of_node, "prevention,large_exit_condition", temp_array, 3);
	if (ret) {
		grip_info->xfsr_normal_exit_thd = 8;
		grip_info->yfsr_normal_exit_thd = 8;
		grip_info->exit_match_thd = 2;
		GRIP_TP_INFO("large exit condition using default.\n");
	} else {
		grip_info->xfsr_normal_exit_thd = temp_array[0];
		grip_info->yfsr_normal_exit_thd = temp_array[1];
		grip_info->exit_match_thd = temp_array[2];
	}

	ret = of_property_read_u32_array(dev->of_node, "prevention,single_channel_width", temp_array, 2);
	if (ret) {
		grip_info->single_channel_x_len = 40;
		grip_info->single_channel_y_len = 40;
		GRIP_TP_INFO("single channel width using default.\n");
	} else {
		grip_info->single_channel_x_len = temp_array[0];
		grip_info->single_channel_y_len = temp_array[1];
	}

	ret = of_property_read_u32_array(dev->of_node, "prevention,normal_tap_condition", temp_array, 2);
	if (ret) {
		grip_info->normal_tap_min_time_ms = 45;
		grip_info->normal_tap_max_time_ms = 150;
		GRIP_TP_INFO("normal tap condition using default.\n");
	} else {
		grip_info->normal_tap_min_time_ms = temp_array[0];
		grip_info->normal_tap_max_time_ms = temp_array[1];
	}

	ret = of_property_read_u32_array(dev->of_node, "prevention,long_edge_condition", temp_array, 4);
	if (ret) {
		normal_data->long_start_coupling_thd = 205;
		normal_data->long_stable_coupling_thd = 95;
		normal_data->long_hold_changed_thd = 160;
		normal_data->long_hold_maxfsr_gap = 200;
		GRIP_TP_INFO("large long edge hold condition using default.\n");
	} else {
		normal_data->long_start_coupling_thd = temp_array[0];
		normal_data->long_stable_coupling_thd = temp_array[1];
		normal_data->long_hold_changed_thd = temp_array[2];
		normal_data->long_hold_maxfsr_gap = temp_array[3];
	}
	transform_normal_para(grip_info);

	ret = of_property_read_u32_array(dev->of_node, "prevention,long_fingerhold_condition", temp_array, 4);
	if (ret) {
		grip_info->long_hold_debounce_time_ms = 100;
		grip_info->long_hold_divided_factor = 6;    //default value, no need to change it
		grip_info->xfsr_hold_exit_thd = 0;
		grip_info->yfsr_hold_exit_thd = 0;
		GRIP_TP_INFO("large long finger hold condition using default.\n");
	} else {
		grip_info->long_hold_debounce_time_ms = temp_array[0];
		grip_info->long_hold_divided_factor = temp_array[1];
		grip_info->xfsr_hold_exit_thd = temp_array[2];
		grip_info->yfsr_hold_exit_thd = temp_array[3];
	}

	ret = of_property_read_u32_array(dev->of_node, "prevention,short_edge_condition", temp_array, 4);
	if (ret) {
		grip_info->short_start_coupling_thd = 201;
		grip_info->short_stable_coupling_thd = 91;
		grip_info->short_hold_changed_thd = 160;
		grip_info->short_hold_maxfsr_gap = 200;
		GRIP_TP_INFO("large short edge hold condition using default.\n");
	} else {
		grip_info->short_start_coupling_thd = temp_array[0];
		grip_info->short_stable_coupling_thd = temp_array[1];
		grip_info->short_hold_changed_thd = temp_array[2];
		grip_info->short_hold_maxfsr_gap = temp_array[3];
	}

	ret = of_property_read_u32_array(dev->of_node, "prevention,top_corner_config", temp_array, 3);
	if (ret) {
		grip_info->large_top_width = 400;
		grip_info->large_top_height = 600;
		grip_info->large_top_exit_distance = 200;
		GRIP_TP_INFO("top corner config using default.\n");
	} else {
		grip_info->large_top_width = temp_array[0];
		grip_info->large_top_height = temp_array[1];
		grip_info->large_top_exit_distance = temp_array[2];
	}

	ret = of_property_read_u32_array(dev->of_node, "prevention,edge_swipe_config", temp_array, 2);
	if (ret) {
		grip_info->edge_swipe_narrow_witdh = 80;
		grip_info->edge_swipe_exit_distance = 300;
		GRIP_TP_INFO("edge swipe config using default.\n");
	} else {
		grip_info->edge_swipe_narrow_witdh = temp_array[0];
		grip_info->edge_swipe_exit_distance = temp_array[1];
	}

	ret = of_property_read_u32_array(dev->of_node, "prevention,trx_strict_reject_condition", temp_array, 3);
	if (ret) {
		grip_info->trx_strict_reject_thd = 7;
		grip_info->rx_strict_reject_thd = 5;
		grip_info->tx_strict_reject_thd = 5;
		GRIP_TP_INFO("trx strict reject using default.\n");
	} else {
		grip_info->trx_strict_reject_thd = temp_array[0];
		grip_info->rx_strict_reject_thd = temp_array[1];
		grip_info->tx_strict_reject_thd = temp_array[2];
	}

	ret = of_property_read_u32_array(dev->of_node, "prevention,long_strict_edge_condition", temp_array, 2);
	if (ret) {
		grip_info->long_strict_start_coupling_thd = 89;
		grip_info->long_strict_stable_coupling_thd = 49;
		GRIP_TP_INFO("long strict edge condition using default.\n");
	} else {
		grip_info->long_strict_start_coupling_thd = temp_array[0];
		grip_info->long_strict_stable_coupling_thd = temp_array[1];
	}


	ret = of_property_read_u32_array(dev->of_node, "prevention,short_strict_edge_condition", temp_array, 2);
	if (ret) {
		grip_info->short_strict_start_coupling_thd = 89;
		grip_info->short_strict_stable_coupling_thd = 49;
		GRIP_TP_INFO("short strict edge condition using default.\n");
	} else {
		grip_info->short_strict_start_coupling_thd = temp_array[0];
		grip_info->short_strict_stable_coupling_thd = temp_array[1];
	}

	ret = of_property_read_u32_array(dev->of_node, "prevention,large_strict_exit_condition", temp_array, 2);
	if (ret) {
		grip_info->xfsr_strict_exit_thd = grip_info->xfsr_normal_exit_thd;
		grip_info->yfsr_strict_exit_thd = grip_info->xfsr_normal_exit_thd;
		GRIP_TP_INFO("large strict exit condition using default.\n");
	} else {
		grip_info->xfsr_strict_exit_thd = temp_array[0];
		grip_info->yfsr_strict_exit_thd = temp_array[1];
	}

	ret = of_property_read_u32_array(dev->of_node, "prevention,corner_move_rejected", temp_array, 1);
	if (ret) {
		grip_info->corner_move_rejected = 0;
		GRIP_TP_INFO("corner move rejected using default.\n");
	} else {
		grip_info->corner_move_rejected = temp_array[0];
	}

	grip_info->corner_eliminate_point_support = of_property_read_bool(dev->of_node, "prevention,corner_eliminate_point_support");
	if (grip_info->corner_eliminate_point_support) {
		GRIP_TP_INFO("corner eliminate point enable.\n");
	}

	grip_info->coord_filter_cnt = makeup_para[0];
	grip_info->coord_buf = kzalloc(TOUCH_MAX_NUM * grip_info->coord_filter_cnt * sizeof(struct coord_buffer), GFP_KERNEL);
	if (grip_info->coord_buf) {
		for (i_index = 0; i_index < TOUCH_MAX_NUM; i_index++) {
			for (j_index = 0; j_index < grip_info->coord_filter_cnt; j_index++) {
				grip_info->coord_buf[i_index * grip_info->coord_filter_cnt + j_index].weight = makeup_para[j_index + 1];
			}
		}
	} else {
		kernel_grip_release(grip_info);
		TPD_INFO("kzalloc grip_zone_area for coord_buffer failed.\n");
		return -1;
	}

	ret = kfifo_alloc(&grip_info->up_fifo, PAGE_SIZE, GFP_KERNEL);
	if (ret) {
		kernel_grip_release(grip_info);
		TPD_INFO("up_fifo malloc failed.\n");
		return -1;
	}

	grip_info->is_curved_screen_v4 = of_property_read_bool(dev->of_node, "prevention,curved_screen_V4")
		|| of_property_read_bool(dev->of_node, "prevention,curved_screen_v4");
	if (grip_info->is_curved_screen_v4) {
	    GRIP_TP_INFO("this is is_curved_screen_v4.\n");
	    ret = kernel_grip_init_v4(grip_info, dev);
	    if (ret < 0) {
		return 0;
	    }
	}

	/* add for v4.3 */
	grip_info->is_curved_screen_v4_3 = of_property_read_bool(dev->of_node, "prevention,curved_screen_v4_3");
	if (grip_info->is_curved_screen_v4_3) {
		GRIP_TP_INFO("this is is_curved_screen_v4_3.\n");
		ret = kernel_grip_init_v4_3(grip_info, dev);
		if (ret < 0) {
			GRIP_TP_INFO("fail to probe is_curved_screen_v4_3\n");
			return 0;
		}
	} else {
		GRIP_TP_INFO("this is is_curved_screen_v4_3 is %d \n", grip_info->is_curved_screen_v4_3);
	}

	/* add for v4.5 */
	grip_info->is_curved_screen_v4_5 = of_property_read_bool(dev->of_node, "prevention,curved_screen_v4_5");
	if (grip_info->is_curved_screen_v4_5) {
		GRIP_TP_INFO("this is is_curved_screen_v4_5.\n");
		ret = kernel_grip_init_v4_5(grip_info, dev);
		if (ret < 0) {
			GRIP_TP_INFO("fail to probe is_curved_screen_v4_5\n");
			return 0;
		}
	} else {
		GRIP_TP_INFO("this is is_curved_screen_v4_5 is %d \n", grip_info->is_curved_screen_v4_5);
	}

	return 0;
}
