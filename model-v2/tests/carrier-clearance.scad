include <../minibox-enclosure.scad>

// Each top-level solid is a failed check. The rendered union must be empty.
assert(carrier_holes == [[17.75, 124], [102.25, 124], [17.75, 17], [102.25, 17]]);
for (i = [0 : 3])
    assert(norm(key_centers[i] - [[78.6, 42.5], [97.65, 42.5], [70.5, 23.45], [89.55, 23.45]][i]) < 0.001);
assert(norm(joystick_center - [24.55, 36.5]) < 0.001);
assert(norm(microphone_center - [52.205, 41.75]) < 0.001);
assert(carrier_mount_z == 9 && carrier_standoff_height == 5 && pcb_thickness == 1.6);
assert(carrier_size == [94.5, 117] && len(carrier_holes) == 4);
assert(screen_mounts == [[-1, -1], [-1, 1], [1, -1]]);
assert(typec_center_x == 48 && typec_center_z == 50.5 &&
       typec_hole_spacing == 20 && typec_hole_diameter == 12);
assert(typec_inner_depth == 30 && typec_inner_diameter == 12);
assert(speaker_size == [27.5, 27.5, 13] && speaker_hole_spacing == [23, 23]);
assert(speaker_grille_diameter == 23.5 && speaker_grille_hole_diameter == 3.8);
assert(speaker_grille_pitch == 6 && speaker_grille_row_pitch == 4.5 &&
       len(speaker_grille_centers) == 14);
assert(speaker_grille_row_counts == [2,3,4,3,2]);
assert(abs(speaker_grille_pitch - speaker_grille_hole_diameter - 2.2) < 0.001);
assert(abs(speaker_post_depth - 3.4) < 0.001 && speaker_ear_thickness == 3.2);
assert(abs(speaker_sleeve_diameter - 7.1) < 0.001 && speaker_front_clearance == 0.2);
assert(abs(speaker_nut_embed_depth - 1.8) < 0.001 &&
       speaker_post_diameter == 4.5 && speaker_screw_length == 6);
assert(minimum_print_wall == 1.2);
assert(socket_height == 8.7 && amplifier_above_socket == 15 && microphone_above_socket == 7.5);
assert(abs(amplifier_stack_height - 23.7) < 0.001);
assert(abs(microphone_stack_height - 16.2) < 0.001);
assert(deck_height == 21.6 && abs(deck_height - carrier_top_z - 11) < 0.001);
assert(slope_base_height == 46 && abs(microphone_cover_top - panel_thickness - carrier_top_z -
       microphone_stack_height - 1.2) < 0.001);
assert(microphone_diameter == 14 && microphone_cover_inner_diameter == 16 &&
       microphone_hole_diameter == 6 && microphone_cover_top == 31);
assert(amplifier_wire_clearance >= 10);
assert(carrier_top_z + microphone_stack_height + 1 <= microphone_cover_top - panel_thickness);
assert(abs(key_window_size - 20) < 0.001 && joystick_window_diameter == 30);
assert(joystick_cap_base_diameter == 26.4);
assert(slope_angle == 65 && deck_depth == 55.8 && bend_radius == 6 && deck_bend_radius == 4);
assert(speaker_center == [104.8,50.3] && speaker_wall_x == 0 && switch_center == [48,79]);
assert(upper_screen_support_angle == 60 && support_angle == 65);
assert(usb_wall_contact_confirmed && usb_occupied_x == [2.4,27.75] &&
       usb_occupied_y == [88,122] && usb_occupied_z == [20.6,32.6]);
assert(joystick_body_height == 12.9);
assert(speaker_tool_tip_diameter == 4 && speaker_tool_space_diameter == 8 &&
       speaker_tool_space_length == 40);
assert(tft_center == [60,34.5]);
assert(profile_count == 7 && rear_height < 110);
assert(slope_length - tangent_length(3) - tft_center[1] - tft_size[1]/2 < 1.3);
assert(slope_base_height + sin(slope_angle) * tft_center[1] <
       31 + sin(80) * 83 - 22);
assert(shell_hole_x == [9,111] && shell_hole_y == [11,125]);
assert(norm(keyboard_window_min - [60.5,13.45]) < 0.001);
assert(norm(keyboard_window_size - [47.15,39.05]) < 0.001);
assert(norm(keyboard_row_bounds[0] - [68.6,32.5,39.05,20]) < 0.001);
assert(norm(keyboard_row_bounds[1] - [60.5,13.45,39.05,20]) < 0.001);
assert(keycap_lateral_clearance == 1 && keycap_vertical_clearance == 1 && key_travel == 4.3);
assert(abs(keycap_top_height - 25.45) < 0.001);
assert(keycap_height == 12 && keycap_size == 18 && keycap_stem_engagement == 2);
assert(switch_outer_diameter == 28 && switch_outer_height == 12);
assert(panel_thickness == 3 && screen_mount_normal == -9 && screen_front_clearance == 1);

// CHECK_GROUP openings
// The display remains a direct aperture in the original 3 mm panel.
intersection() {
    bottom_shell();
    on_slope(107.96,32.52,-2.88) cube([8.0,3.96,1.36]);
}
// Retain solid panel material on both sides of the display, through its full thickness.
difference() {
    for(x=[8,110]) on_slope(x,20,-2.98) cube([2,25,2.96]);
    lid();
}
intersection() {
    lid();
    union() {
        translate([68.61,32.51,18.5]) cube([39.03,19.98,18.55]);
        translate([60.51,13.46,18.5]) cube([39.03,19.98,18.55]);
        translate([70,32.6,18.5]) cube([28,0.75,18.55]);
    }
}
intersection() {
    lid();
    translate([24.55, 36.5, 18.5]) cylinder(d = 29.98, h = rear_height);
}
// Material reclaimed from the old 35 mm bore must be present on both sides.
difference() {
    for(x=[8.55,40.55]) translate([x-0.1,36.4,18.7]) cube([0.2,0.2,2.8]);
    lid();
}
intersection() {
    lid();
    union() {
        translate([52.205, 41.75, 18.5]) cylinder(d = 15.98, h = 9.49);
        translate([52.205, 41.75, 27.9]) cylinder(d = 5.98, h = 3.2);
    }
}
difference() {
    translate([52.205, 41.75, 18.7])
        difference() {
            cylinder(d = 20.6, h = 12.2);
            translate([0, 0, -eps]) cylinder(d = 16.2, h = 12.2 + 2 * eps);
        }
    lid();
}
difference() {
    translate([52.205, 41.75, 28.1])
        difference() {
            cylinder(d = 15.8, h = 2.8);
            translate([0, 0, -eps]) cylinder(d = 6.2, h = 2.8 + 2 * eps);
        }
    lid();
}
// Keep both staggered shoulders: an enclosing rectangle must fail these positive probes.
difference() {
    union() {
        for (p = [[62,35], [67,44], [101,16], [106,30]])
            translate([p[0],p[1],18.7]) cube([0.4,0.4,2.8]);
        for (p = [[60.3,30], [107.85,30], [84,13.25]])
            translate([p[0] - 0.1, p[1] - 0.1, 18.7]) cube([0.2, 0.2, 2.8]);
        translate([55,50.5,18.7]) cube([2,0.8,2.8]);
    }
    lid();
}
intersection() {
    bottom_shell();
    for (x = [38, 58])
        translate([x, case_depth + 0.1, typec_center_z])
            rotate([90, 0, 0]) cylinder(d = 11.98, h = wall + 0.2);
}
// The former switch position is closed, not retained as a fourth rear opening.
difference() {
    translate([81, case_depth - wall + 0.1, 65])
        cube([2, wall - 0.2, 2]);
    bottom_shell();
}
// Reclaimed material at the previous crowded triangle's leftmost socket and switch.
difference() {
    for(p=[[26,54.9],[33,79]])
        translate([p[0]-0.5,case_depth-wall+0.1,p[1]-0.5]) cube([1,wall-0.2,1]);
    bottom_shell();
}
// The old right-side grille and nut pockets leave an intact wall.
difference() {
    translate([case_width-wall+0.1,94,28]) cube([wall-0.2,28,28]);
    bottom_shell();
}
intersection() {
    bottom_shell();
    for (row=[-2:2], column=[0:3-abs(row)])
        let(count=4-abs(row),
            p=[6*(column-(count-1)/2),4.5*row])
            on_speaker(104.8+p[0],50.3+p[1],-3.1) cylinder(d=3.78,h=3.2);
}
// The solid plate between vents must remain; a single large circular cut fails this probe.
difference() {
    for (p=[[0,0],[6,0],[-6,0]])
        on_speaker(104.8+p[0]-0.1,50.3+p[1]-0.1,-2.9) cube([0.2,0.2,2.8]);
    bottom_shell();
}
intersection() {
    bottom_shell();
    translate([48,136.02,79]) rotate([90,0,0])
        cylinder(d = 20.98, h = 2.44);
}
intersection() {
    bottom_shell();
    for (row=[-2:2], column=[0:3-abs(row)])
        let(count=4-abs(row),p=[6*(column-(count-1)/2),4.5*row])
            translate([-0.1,104.8+p[0],50.3+p[1]]) rotate([0,90,0])
                cylinder(d=3.78,h=3.2);
}
// Keep the whole riser and upper transition intact.
difference() {
    intersection() {
        extrude_along_x(case_width) panel_band(eps, panel_thickness-eps);
        translate([7,55,25.7]) cube([106,10,20.3]);
    }
    lid();
}
difference() {
    intersection() {
        extrude_along_x(case_width) panel_band(eps, minimum_print_wall);
        translate([7,55,25.7]) cube([106,10,20.3]);
    }
    lid();
}
// No control cutter may reach either the vertical straight segment or the inclined screen.
intersection() {
    control_vertical_openings();
    translate([0,0,25.7]) cube([120,136,120]);
}
intersection() {
    lid();
    on_slope(tft_center[0] + screen_window_offset[0] - 42.75,
             tft_center[1] - 22.8, -3.1) cube([85.5, 45.6, 5]);
}
intersection() {
    lid();
    shell_fasteners() translate([0, 0, -eps]) cylinder(d = 2.98, h = 3.1);
}
difference() {
    shell_fasteners() translate([0, 0, eps])
        difference() {
            cylinder(d = 4.48, h = 1.6 - 2 * eps);
            translate([0, 0, -eps]) cylinder(d = 3.02, h = 1.6);
        }
    lid();
}

// CHECK_GROUP mounts
// The removed intermediate top screws leave a solid lid and no hanging sockets below it.
difference() {
    for (x=[9,111])
        translate([x,110,rear_height-2.9]) cylinder(d=4.48,h=2.8);
    lid();
}
intersection() {
    bottom_shell();
    for (x=[9,111])
        translate([x-2,108,rear_height-panel_thickness-9.8]) cube([4,4,9.6]);
}
intersection() {
    bottom_shell();
    carrier_fasteners() translate([0, 0, -5.18]) cylinder(d = 4.68, h = 5.2);
}
difference() {
    union() {
        carrier_fasteners() translate([2.8, -0.1, -0.5]) cube([0.2, 0.2, 0.4]);
        carrier_fasteners() translate([-0.5, -0.5, -8.5]) cube([1, 1, 3]);
    }
    bottom_shell();
}
// There are no independent keyboard, joystick, expansion-board posts or breadboard.
intersection() {
    bottom_shell();
    translate([15, 14, floor_thickness + 0.1])
        linear_extrude(height = carrier_mount_z - floor_thickness - 0.2)
            difference() {
                square([90, 114]);
                for (p = carrier_holes) translate(p - [15, 14]) circle(d = 7.1);
            }
}
intersection() {
    bottom_shell();
    screen_fasteners() translate([0, 0, -5.18]) cylinder(d = 4.68, h = 5.2);
}
difference() {
    screen_fasteners() translate([3, -0.1, -0.3]) cube([0.2, 0.2, 0.2]);
    bottom_shell();
}
// Probe vertically BELOW each retained screen hole, not only behind its tilted axis.
difference() {
    union()
        for (p = screen_mounts)
            let(x = tft_center[0] + p[0] * 50.95,
                along = tft_center[1] + p[1] * 27.45)
                translate([x - 0.5, deck_depth + cos(65) * along -
                           sin(65) * screen_mount_normal - 0.5,
                           slope_base_height + sin(65) * along +
                           cos(65) * screen_mount_normal - 6]) cube([1, 1, 1]);
    bottom_shell();
}
// Entire former upper-right pad, not just its screw hole, must be absent.
intersection() {
    bottom_shell();
    on_slope(110.95, tft_center[1] + 27.45, screen_mount_normal - 9.9)
        cylinder(d = 7.9, h = 9.8);
}
intersection() {
    bottom_shell();
    speaker_posts(eps);
}
difference() {
    union() {
        speaker_fasteners() translate([2.8, -0.1, speaker_post_depth + 0.1])
            cube([0.2, 0.2, 1.4]);
        speaker_fasteners() translate([-0.4, -0.4, insert_depth + eps])
            cube([0.8, 0.8, 1.2 - 2 * eps]);
    }
    bottom_shell();
}

// CHECK_GROUP assembly
// Independent proof that the new 30 mm inner-body/tail depth is present in the inspection envelope.
difference() {
    for(x=[38,58])
        translate([x,133.58,50.5]) rotate([90,0,0]) cylinder(d=11.96,h=29.96);
    typec_envelopes();
}
// Cables are installed after the PCB is seated; they are not part of the tilted insertion load.
intersection() {
    usb_cable_envelope(-assembly_clearance);
    union() {
        // The side wall is an intentional user-confirmed contact; mounts are not.
        screen_tabs(); shell_mount_columns(); lid(); speaker_envelope(eps); speaker_sleeves();
        screen_envelope(eps); switch_envelope(eps); typec_envelopes(eps);
        carrier_board(eps);
        for (i=[1:13]) carrier_component_envelope(i,eps);
    }
}
intersection() { usb_cable_envelope(eps); bottom_shell(); }
// The narrow tip fits the speaker's own well; the full working corridor stays free with hardware installed.
intersection() { speaker_tool_envelope(eps); speaker_body_envelope(eps); }
intersection() {
    speaker_tool_envelope(-assembly_clearance);
    union() {
        bottom_shell(); lid(); typec_envelopes(eps); switch_envelope(eps);
        module_envelopes(eps);
    }
}
// Independent tool bounds in world coordinates prevent an inverted or missing driver reservation.
difference() {
    for(y=[93.3,116.3],z=[38.8,61.8])
        translate([16.62,y,z]) rotate([0,90,0]) cylinder(d=7.96,h=39.96);
    speaker_tool_envelope();
}
difference() {
    translate([2.42,88.02,20.62]) cube([25.31,33.96,11.96]);
    usb_cable_envelope();
}
// Independent YTL body regions must remain represented; do not shrink the cap to hide the body.
difference() {
    union() {
        translate([16.47,29.445,10.62]) cube([17.96,16.935,12.86]);
        translate([19.97,24.42,10.62]) cube([9.16,5.985,12.86]);
    }
    joystick_body_envelope();
}
// Exclude the intended normal contact with the support lip, including the vertical riser.
// A global Z shift cannot separate coincident vertical mating faces.
intersection() {
    bottom_shell();
    intersection() {
        lid();
        union() {
            extrude_along_x(case_width) panel_band(eps, panel_thickness-eps);
            microphone_cover();
        }
    }
}
intersection() { module_envelopes(eps); bottom_shell(); }
intersection() { module_envelopes(eps); lid(); }
intersection() { keycap_swept_envelope(eps); lid(); }
intersection() { keycap_swept_envelope(eps); bottom_shell(); }
intersection() { keycap_swept_envelope(eps); switch_outer_envelope(eps); }
intersection() { switch_envelope(eps); bottom_shell(); }
intersection() { switch_envelope(eps); lid(); }
intersection() { switch_envelope(eps); module_envelopes(eps); }
intersection() { switch_envelope(-assembly_clearance); module_envelopes(eps); }
// External flange/rocker swept envelope versus released keys, not only internal terminals.
intersection() { switch_outer_envelope(-assembly_clearance); carrier_components(eps); }
intersection() { switch_outer_envelope(eps); lid(); }
intersection() { switch_outer_envelope(eps); bottom_shell(); }
assert(79 - 12 - (carrier_top_z + rtc_stack_height) > 16);
assert(rtc_stack_height == 40);
intersection() { screen_envelope(eps); carrier_components(-assembly_clearance); }
intersection() { screen_envelope(eps); carrier_board(eps); }
intersection() { switch_envelope(-assembly_clearance); carrier_component_envelope(2,eps); }
intersection() { speaker_envelope(-assembly_clearance); carrier_component_envelope(2,eps); }
intersection() { speaker_envelope(eps); bottom_shell(); }
intersection() { speaker_envelope(eps); lid(); }
intersection() { speaker_envelope(eps); module_envelopes(eps); }
intersection() { speaker_envelope(-assembly_clearance); module_envelopes(eps); }
intersection() { speaker_envelope(eps); switch_envelope(eps); }
intersection() { speaker_posts(eps); speaker_body_envelope(eps); }
intersection() { speaker_posts(eps); lid(); }
intersection() { typec_envelopes(eps); bottom_shell(); }
intersection() { typec_envelopes(eps); lid(); }
intersection() { typec_envelopes(eps); module_envelopes(eps); }
intersection() { typec_envelopes(eps); speaker_envelope(eps); }
intersection() { typec_envelopes(eps); switch_envelope(eps); }
intersection() { typec_envelopes(-assembly_clearance); switch_envelope(eps); }
intersection() { typec_envelopes(-assembly_clearance); switch_outer_envelope(eps); }
intersection() { typec_envelopes(-assembly_clearance); speaker_envelope(eps); }
intersection() { typec_envelopes(-assembly_clearance); speaker_sleeves(); }
intersection() { typec_envelopes(-assembly_clearance); module_envelopes(eps); }
intersection() { typec_envelopes(-assembly_clearance); shell_mount_columns(); }
intersection() { speaker_mount_envelope(assembly_clearance); shell_mount_columns(); }
intersection() { speaker_envelope(-assembly_clearance); switch_envelope(eps); }
// Panel hardware needs clearance from structural mounts, except its intended seats.
intersection() { speaker_envelope(-assembly_clearance); screen_tabs(); }
intersection() { switch_envelope(-assembly_clearance); screen_tabs(); }
intersection() {
    speaker_mount_envelope(assembly_clearance);
    union() { screen_tabs(); screen_envelope(eps); carrier_components(eps); switch_envelope(eps); }
}
intersection() {
    screen_tabs();
    carrier_components(-assembly_clearance);
}
// Explicit connector checks: include all 12 mm of the screen-side connector/wire reservation.
intersection() { screen_header_envelope(-assembly_clearance); carrier_component_envelope(2,eps); }
intersection() { screen_header_envelope(-assembly_clearance); speaker_envelope(eps); }
intersection() { screen_header_envelope(-assembly_clearance); speaker_sleeves(); }
intersection() {
    screen_header_envelope(-assembly_clearance);
    translate([0,case_depth-wall,0]) cube([case_width,wall,rear_height]);
}
// At least 1 mm around the entire amplifier/terminal/wiring stack, not only its PCB.
intersection() {
    amplifier_envelope(-assembly_clearance);
    union() { bottom_shell(); lid(); switch_envelope(); speaker_envelope(); typec_envelopes(); }
}
// A purchased cap must fit this complete swept envelope; its static diameter alone is insufficient.
assert(joystick_motion_diameter == 29 && joystick_total_height == 30);
assert(joystick_window_diameter - joystick_motion_diameter >= 1);

// CHECK_GROUP shell
// Only the microphone cover may leave the original panel band.
difference() {
    lid();
    extrude_along_x(case_width) panel_band(-eps, panel_thickness + eps);
    microphone_cover();
}
difference() { bottom_shell(); case_volume(-eps); }
intersection() {
    bottom_shell();
    intersection() {
        translate([15, 0, 0]) extrude_along_x(case_width - 30) panel_band(0, panel_thickness + 0.2);
        plan_volume() opening_plan(support_lip_width + eps);
    }
}
intersection() {
    bottom_shell();
    intersection() {
        extrude_along_x(case_width) panel_band(eps, panel_thickness - eps);
        plan_volume() difference() {
            opening_plan(eps);
            offset(delta = eps) lid_plan();
        }
    }
}
difference() {
    intersection() {
        extrude_along_x(case_width)
            panel_band(panel_thickness + eps, panel_thickness + support_lip_thickness - eps);
        plan_volume() difference() {
            opening_plan(eps);
            opening_plan(support_lip_width - eps);
        }
    }
    bottom_shell();
}
