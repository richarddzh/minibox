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
assert(typec_center_x == 38 && typec_hole_spacing == 20 && typec_hole_diameter == 12);
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
assert(slope_base_height == 31 && abs(microphone_cover_top - panel_thickness - carrier_top_z -
       microphone_stack_height - 1.2) < 0.001);
assert(microphone_diameter == 14 && microphone_cover_inner_diameter == 16 &&
       microphone_hole_diameter == 6 && microphone_cover_top == 31);
assert(amplifier_wire_clearance >= 10);
assert(carrier_top_z + microphone_stack_height + 1 <= microphone_cover_top - panel_thickness);
assert(abs(key_window_size - 20) < 0.001 && joystick_window_diameter == 35);
assert(slope_angle == 80 && deck_depth == 52 && bend_radius == 6);
assert(speaker_center == [26,30] && switch_center == [90,34]);
assert(tft_center == [60,83]);
assert(shell_hole_x == [9,111] && shell_hole_y == [11,125]);
assert(norm(keyboard_window_min - [60.5,13.45]) < 0.001);
assert(norm(keyboard_window_size - [47.15,39.05]) < 0.001);
assert(norm(keyboard_row_bounds[0] - [68.6,32.5,39.05,20]) < 0.001);
assert(norm(keyboard_row_bounds[1] - [60.5,13.45,39.05,20]) < 0.001);
assert(keycap_lateral_clearance == 1 && keycap_vertical_clearance == 1 && key_travel == 4.3);
assert(abs(keycap_top_height - 25.45) < 0.001);
assert(keycap_height == 12 && keycap_size == 18 && keycap_stem_engagement == 2);
assert(switch_outer_diameter == 28 && switch_outer_height == 12);

// CHECK_GROUP openings
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
    translate([24.55, 36.5, 18.5]) cylinder(d = 34.98, h = rear_height);
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
        on_slope(60, 22, -2.9) cylinder(d = 4.8, h = 2.8);
    }
    lid();
}
intersection() {
    bottom_shell();
    for (x = [28, 48])
        translate([x, case_depth + 0.1, typec_center_z])
            rotate([90, 0, 0]) cylinder(d = 11.98, h = wall + 0.2);
}
// The former right-hand rear port must be solid, not left behind as a third hole.
difference() {
    translate([70 - 1, case_depth - wall + 0.1, typec_center_z - 1])
        cube([2, wall - 0.2, 2]);
    bottom_shell();
}
intersection() {
    lid();
    for (row=[-2:2], column=[0:3-abs(row)])
        let(count=4-abs(row),
            p=[6*(column-(count-1)/2),4.5*row])
            on_slope(26+p[0],30+p[1],-3.1) cylinder(d=3.78,h=3.2);
}
// The solid plate between vents must remain; a single large circular cut fails this probe.
difference() {
    for (p=[[0,0],[6,0],[-6,0]])
        on_slope(26+p[0]-0.1,30+p[1]-0.1,-2.9) cube([0.2,0.2,2.8]);
    lid();
}
intersection() {
    lid();
    on_slope(switch_center[0], switch_center[1], -panel_thickness - eps)
        cylinder(d = switch_hole_diameter - 0.02, h = panel_thickness + 2 * eps);
}
intersection() {
    lid();
    on_slope(tft_center[0] + screen_window_offset[0] - 42.75,
             tft_center[1] - 22.8, -3.1) cube([85.5, 45.6, 3.2]);
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
                translate([x - 0.5, deck_depth + cos(80) * along -
                           sin(80) * screen_mount_normal - 0.5,
                           slope_base_height + sin(80) * along +
                           cos(80) * screen_mount_normal - 6]) cube([1, 1, 1]);
    bottom_shell();
}
// Entire former upper-right pad, not just its screw hole, must be absent.
intersection() {
    bottom_shell();
    on_slope(110.95, tft_center[1] + 27.45, screen_mount_normal - 9.9)
        cylinder(d = 7.9, h = 9.8);
}
intersection() {
    lid();
    speaker_posts(eps);
}
difference() {
    union() {
        speaker_fasteners() translate([2.8, -0.1, speaker_post_depth + 0.1])
            cube([0.2, 0.2, 1.4]);
        speaker_fasteners() translate([-0.4, -0.4, insert_depth + eps])
            cube([0.8, 0.8, 1.2 - 2 * eps]);
    }
    lid();
}

// CHECK_GROUP assembly
intersection() { bottom_shell(); translate([0, 0, eps]) lid(); }
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
assert(slope_base_height + sin(80) * (34 - 14) - (carrier_top_z + 25.45) > 11.6);
intersection() { speaker_envelope(eps); bottom_shell(); }
intersection() { speaker_envelope(eps); lid(); }
intersection() { speaker_envelope(eps); module_envelopes(eps); }
intersection() { speaker_envelope(-assembly_clearance); module_envelopes(eps); }
intersection() { speaker_envelope(eps); switch_envelope(eps); }
intersection() { speaker_posts(eps); speaker_body_envelope(eps); }
intersection() { speaker_posts(eps); bottom_shell(); }
intersection() { typec_envelopes(eps); bottom_shell(); }
intersection() { typec_envelopes(eps); lid(); }
intersection() { typec_envelopes(eps); module_envelopes(eps); }
intersection() { typec_envelopes(eps); speaker_envelope(eps); }
intersection() { typec_envelopes(eps); switch_envelope(eps); }
// At least 1 mm around the entire amplifier/terminal/wiring stack, not only its PCB.
intersection() {
    amplifier_envelope(-assembly_clearance);
    union() { bottom_shell(); lid(); switch_envelope(); speaker_envelope(); typec_envelopes(); }
}
// A purchased cap must fit this complete swept envelope; its static diameter alone is insufficient.
assert(joystick_motion_diameter == 34 && joystick_total_height == 30);
assert(joystick_window_diameter - joystick_motion_diameter >= 1);

// CHECK_GROUP shell
// Only the microphone cover and integral speaker sleeves may leave the panel band.
difference() {
    lid();
    extrude_along_x(case_width) panel_band(-eps, panel_thickness + eps);
    microphone_cover();
    speaker_sleeves();
}
// Cleanup is confined to the inner 1.2 mm, with no new exterior aperture.
difference() {
    control_thin_edge_cleanup();
    extrude_along_x(case_width)
        panel_band(panel_thickness - minimum_print_wall - eps, panel_thickness + 2 * eps);
}
intersection() {
    lid();
    control_thin_edge_cleanup();
}
difference() {
    for (bounds = [[7.65,33.8],[67.5,41.25]])
        on_slope(bounds[0],12.2,-2.9) cube([bounds[1],1.2,2.8]);
    lid();
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
