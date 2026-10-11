include <../minibox-enclosure.scad>

mesh_path = "";
stage = "all"; // [all, lower, level, align, seat, rtc, strength, straight]
entry_roll = 25;
turn_height = 35;
forward_shift = -1;
step_angle = 2.5;
motion_margin = 0.1;

assert(mesh_path != "", "Supply the validated bottom STL with mesh_path.");
assert(stage == "all" || stage == "lower" || stage == "level" || stage == "align" ||
       stage == "seat" || stage == "rtc" || stage == "strength" || stage == "straight",
       "Unknown installation stage.");
assert(entry_roll == 25 && turn_height == 35 && forward_shift == -1);
assert(carrier_size == [94.5,117] && pcb_thickness == 1.6 && solder_tail_depth == 3.5);
radius_bound = norm([case_width / 2,
    pcb_thickness + solder_tail_depth +
    max(esp32_stack_height, amplifier_stack_height + amplifier_wire_clearance,
        socket_height + 10, microphone_stack_height, keycap_top_height, 19.45)]);
assert(radius_bound * (1 - cos(step_angle / 2)) < motion_margin,
       "Rotation interpolation needs a larger swept-envelope margin.");

module shell_mesh() {
    import(mesh_path);
}

module moving_component(index, inset = 0) {
    if (index == 0) carrier_board(inset);
    else if (index == 1) carrier_solder_envelope(inset);
    else carrier_component_envelope(index - 2, inset, inserting = true);
}

module at_pose(z, roll, dy) {
    centre = [case_width / 2, carrier_front_y + carrier_size[1] / 2];
    translate([centre[0], centre[1] + dy, z])
        rotate([0, roll, 0])
            translate([-centre[0], -centre[1], -carrier_mount_z]) children();
}

module segment(z1, a1, dy1, z2, a2, dy2, inset) {
    for (index = [0 : 15])
        difference() {
            // Per-component hulls do not invent material between separate tall modules.
            hull() {
                at_pose(z1, a1, dy1) moving_component(index, inset);
                at_pose(z2, a2, dy2) moving_component(index, inset);
            }
            // Convex hulling fills solder keepouts. Restore only the four intended mount contacts.
            if (index == 1 && a1 == 0 && a2 == 0 && dy1 == 0 && dy2 == 0)
                for (p = carrier_holes)
                    translate([p[0], p[1], 0])
                        cylinder(d = carrier_boss_diameter + 0.02, h = rear_height);
        }
}

intersection() {
    shell_mesh();
    union() {
        if (stage == "all" || stage == "lower")
            segment(rear_height + 30, entry_roll, forward_shift,
                    turn_height, entry_roll, forward_shift, -motion_margin);
        if (stage == "all" || stage == "level")
            for (a = [entry_roll : -step_angle : step_angle])
                segment(turn_height, a, forward_shift,
                        turn_height, a - step_angle, forward_shift, -motion_margin);
        if (stage == "all" || stage == "align")
            segment(turn_height, 0, forward_shift, turn_height, 0, 0, -motion_margin);
        if (stage == "all" || stage == "seat") {
            segment(turn_height, 0, 0, carrier_mount_z + 0.2, 0, 0, -motion_margin);
            segment(carrier_mount_z + 0.2, 0, 0, carrier_mount_z + eps, 0, 0, eps);
        }
        // Diagnostic only: flat vertical insertion remains intentionally unsupported.
        if (stage == "straight")
            hull() {
                at_pose(rear_height + 30, 0, 0) carrier_board(eps);
                carrier_board(eps);
            }
    }
}

// RTC is brought through the open front BELOW the rear roof, then lowered onto its own socket.
// Its full installed reservation conservatively covers the module/pins during the move.
if (stage == "all" || stage == "rtc")
    intersection() {
        union() {
            shell_mesh();
            carrier_board(eps);
            for (index = [0 : 13])
                if (index != 2) carrier_component_envelope(index, eps, inserting = true);
        }
        union() {
            hull() {
                translate([0,-140,0]) pcb_box([60.5,3,88,15],rtc_stack_height,-motion_margin,55);
                pcb_box([60.5,3,88,15],rtc_stack_height,-motion_margin,55);
            }
            hull() {
                pcb_box([60.5,3,88,15],rtc_stack_height,-motion_margin,55);
                pcb_box([60.5,3,88,15],rtc_stack_height,-motion_margin,carrier_top_z+0.2);
            }
            hull() {
                pcb_box([60.5,3,88,15],rtc_stack_height,eps,carrier_top_z+0.2);
                pcb_box([60.5,3,88,15],rtc_stack_height,eps);
            }
        }
    }

if (stage == "all" || stage == "strength")
    difference() {
        union() {
            for (x = shell_hole_x) {
                translate([x,11,deck_height-panel_thickness-insert_depth+eps])
                    difference() {
                        cylinder(d=6.8,h=insert_depth-2*eps);
                        translate([0,0,-eps]) cylinder(d=4.8,h=insert_depth);
                    }
                translate([x,11,11.62]) cylinder(d=4.7,h=7.56);
            }
        }
        shell_mesh();
    }
