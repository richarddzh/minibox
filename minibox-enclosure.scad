/*
  Sketch-based enclosure. Units: mm.
  X = left/right, Y = front/back, Z = up.
  Both halves use the same tangent, rounded side profile.
  Unmeasured radii, component heights and control windows are provisional.
*/

/* [View] */
part = "exploded"; // [assembly, exploded, bottom, lid, lid-print]
show_modules = false;
explode_height = 35;
$fn = 64;

/* [Enclosure] */
case_width = 120;
deck_depth = 58;
deck_height = 28;
slope_angle = 60;
slope_length = 92;
rear_ledge = 18;
wall = 2.4;
floor_thickness = 4;
panel_thickness = 3;
joint_clearance = 0.35;

/* [Rounded corners] */
bottom_radius = 6;
rim_radius = 6;
bend_radius = 10;

/* [M2.5 fasteners] */
screw_clearance = 2.8;
lid_screw_diameter = 3;
screw_length = 10;
insert_diameter = 4.7;
insert_depth = 5.2;
screw_tip_depth = 8.8;
control_standoff_height = 6.5;
module_boss_diameter = 8;
screen_tab_depth = 10;
shell_boss_diameter = 9;
screen_gusset_thickness = 2;
small_gap_fill = 5;

/* [Controls: joystick left, keyboard right] */
joystick_size = [30, 40];
joystick_hole_spacing = [21, 28];
joystick_center = [31, 30];
joystick_window = [24, 24];
joystick_active_size = [22, 22];
joystick_total_height = 30;
joystick_component_height = 7;
keyboard_size = [57.4, 27.2, 20.6];
keyboard_hole_spacing = [53.34, 22.86];
keyboard_center = [84, 32];
keyboard_active_size = [keyboard_size[0], keyboard_size[1]];
keyboard_window_clearance = 1;
keyboard_window = [keyboard_size[0] + 2 * keyboard_window_clearance,
                   keyboard_size[1] + 2 * keyboard_window_clearance];
keyboard_component_height = 7;
pcb_thickness = 1.6;
module_clearance = 1;
control_clearance = 2;

/* [Landscape TFT: dimensions rotated together] */
tft_size = [108.04, 61.74];
tft_view = [83.52, 55.68];
tft_hole_spacing = [101.9, 54.9];
tft_window_clearance = 0.8;
screen_stack_above_pcb = 8;
screen_front_clearance = 1;
screen_back_height = 4;
screen_back_edge_margin = 8;
screen_window_offset = [0, 0];

/* [Optional cable opening: absent in sketch] */
enable_cable_opening = false;
cable_opening = [16, 10];
cable_opening_z = 10;

/* [Hidden] */
eps = 0.02;
screen_top_y = deck_depth + cos(slope_angle) * slope_length;
rear_height = deck_height + sin(slope_angle) * slope_length;
screen_board_gap = screen_stack_above_pcb + screen_front_clearance;
screen_mount_normal = -panel_thickness - screen_board_gap - pcb_thickness;
tft_center = [case_width / 2, slope_length / 2];
tft_rear_extent = deck_depth + cos(slope_angle) * (tft_center[1] + tft_size[1] / 2) -
                  sin(slope_angle) * screen_mount_normal;
case_depth = ceil(max(screen_top_y + rear_ledge,
                      tft_rear_extent + 10 + shell_boss_diameter / 2 + module_clearance));
keyboard_mount_z = floor_thickness + control_standoff_height;
joystick_mount_z = floor_thickness + control_standoff_height;
joystick_board_gap = deck_height - panel_thickness - joystick_mount_z - pcb_thickness;
shell_hole_x = [8, case_width - 8];
shell_hole_y = [10, case_depth - 10];
profile_vertices = [
    [0, 0], [case_depth, 0], [case_depth, rear_height],
    [screen_top_y, rear_height], [deck_depth, deck_height], [0, deck_height]
];
profile_radii = [
    bottom_radius, bottom_radius, rim_radius,
    bend_radius, bend_radius, rim_radius
];

function unit(v) = v / norm(v);
function cross2(a, b) = a[0] * b[1] - a[1] * b[0];
function column_bounds(x, y) = [
    x < case_width / 2 ? 0 : x - shell_boss_diameter / 2,
    y < deck_depth ? 0 : y - shell_boss_diameter / 2,
    x < case_width / 2 ? x + shell_boss_diameter / 2 : case_width,
    y < deck_depth ? y + shell_boss_diameter / 2 : case_depth
];
function distance_to_rectangle(p, bounds) = norm([
    max(bounds[0] - p[0], 0, p[0] - bounds[2]),
    max(bounds[1] - p[1], 0, p[1] - bounds[3])
]);
function reverse_list(a) = [for (i = [len(a) - 1 : -1 : 0]) a[i]];
function corner_angle(i) =
    let(p = profile_vertices[i],
        u = unit(profile_vertices[(i + 5) % 6] - p),
        v = unit(profile_vertices[(i + 1) % 6] - p))
    acos(max(-1, min(1, u * v)));
function tangent_length(i) = profile_radii[i] / tan(corner_angle(i) / 2);

function corner_points(i) =
    let(p = profile_vertices[i],
        u = unit(profile_vertices[(i + 5) % 6] - p),
        v = unit(profile_vertices[(i + 1) % 6] - p),
        angle = corner_angle(i),
        radius = profile_radii[i],
        center = p + unit(u + v) * radius / sin(angle / 2),
        start = p + u * tangent_length(i),
        a0 = atan2(start[1] - center[1], start[0] - center[0]),
        sweep = sign(cross2(-u, v)) * (180 - angle),
        steps = max(4, ceil(abs(sweep) / 3)))
    [for (j = [0 : steps])
        center + radius * [cos(a0 + sweep * j / steps), sin(a0 + sweep * j / steps)]];

function top_path(trim = 0) = concat(
    [[rim_radius + trim, deck_height]],
    reverse_list(corner_points(4)),
    reverse_list(corner_points(3)),
    [[case_depth - rim_radius - trim, rear_height]]
);

function inward_normal(v) = let(d = unit(v)) [d[1], -d[0]];
// Offset the sampled path with miters so the two halves share identical facets.
function inner_path(path, depth) = [
    for (i = [0 : len(path) - 1])
        let(n1 = inward_normal(path[max(1, i)] - path[max(0, i - 1)]),
            n2 = inward_normal(path[min(len(path) - 1, i + 1)] -
                               path[min(len(path) - 2, i)]))
        path[i] + depth * (n1 + n2) / (1 + n1 * n2)
];

assert(part == "assembly" || part == "exploded" || part == "bottom" ||
       part == "lid" || part == "lid-print" || part == "none", "Unknown part.");
assert(slope_angle > 30 && slope_angle < 80, "Screen slope must be between 30 and 80 degrees.");
assert(min(bottom_radius, rim_radius) > wall && bend_radius > panel_thickness,
       "Corner radii must exceed their wall/panel thickness.");
assert(floor_thickness >= wall, "Floor thickness must be at least the wall thickness.");
assert(joint_clearance > 0 && joint_clearance < wall, "Invalid lid clearance.");
assert(control_standoff_height >= insert_depth + 1,
       "Low control posts need material below the glued nut pockets.");
assert(min(keyboard_mount_z, joystick_mount_z) - screw_tip_depth >= 1.5,
       "Thicken the floor: the 10 mm screws must leave at least 1.5 mm below the tip pockets.");
assert(screen_front_clearance >= module_clearance, "Insufficient screen-to-lid clearance.");
assert(keyboard_mount_z + pcb_thickness + keyboard_component_height + module_clearance <
       deck_height - panel_thickness, "Keyboard body hits the lid.");
assert(joystick_component_height + module_clearance < joystick_board_gap,
       "Joystick body hits the lid.");
assert(keyboard_window[0] > keyboard_active_size[0] &&
       keyboard_window[1] > keyboard_active_size[1] &&
       joystick_window[0] > joystick_active_size[0] &&
       joystick_window[1] > joystick_active_size[1], "Control windows need motion clearance.");
assert(keyboard_center[0] - keyboard_size[0] / 2 -
       joystick_center[0] - joystick_size[0] / 2 >= control_clearance,
       "Keyboard and joystick modules overlap.");
assert(joystick_center[0] - joystick_size[0] / 2 >=
       shell_hole_x[0] + shell_boss_diameter / 2 + control_clearance &&
       keyboard_center[1] - keyboard_size[1] / 2 >=
       shell_hole_y[0] + shell_boss_diameter / 2 + control_clearance,
       "Controls are too close to the front lid columns.");
assert(screen_tab_depth > screw_tip_depth &&
       deck_height - panel_thickness - floor_thickness > screw_tip_depth,
       "Mounting tabs need blind clearance for the screw tips.");
assert(screen_gusset_thickness >= 2, "Screen gussets must be at least 2 mm thick.");
assert(screw_length - pcb_thickness <= screw_tip_depth &&
       screw_length - panel_thickness <= screw_tip_depth,
       "Screws are too long for the blind holes.");
assert(insert_depth < screw_tip_depth && insert_diameter < module_boss_diameter - 2,
       "Insufficient insert depth or boss wall.");
assert(tft_size[0] + 2 * (wall + joint_clearance) < case_width,
       "Landscape screen does not fit between the side walls.");
assert(tft_size[1] + 2 * tangent_length(3) + 8 < slope_length,
       "TFT overlaps the curved transitions.");
assert(rear_ledge > tangent_length(2) + tangent_length(3) + 4,
       "Rear ledge is too short.");
for (i = [0 : 5])
    assert(tangent_length(i) + tangent_length((i + 1) % 6) <
           norm(profile_vertices[(i + 1) % 6] - profile_vertices[i]),
           "Fillets overlap: reduce radii or enlarge the profile.");
for (spec = [[joystick_center, joystick_size], [keyboard_center, keyboard_size]])
    assert(spec[0][0] - spec[1][0] / 2 > wall &&
           spec[0][0] + spec[1][0] / 2 < case_width - wall &&
           spec[0][1] - spec[1][1] / 2 > rim_radius &&
           spec[0][1] + spec[1][1] / 2 < deck_depth - tangent_length(4),
           "Control PCB overlaps a wall or the lower curved transition.");

module extrude_along_x(length) {
    multmatrix([[0, 0, 1, 0], [1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 0, 1]])
        linear_extrude(height = length) children();
}

module extrude_along_y(length) {
    multmatrix([[1, 0, 0, 0], [0, 0, 1, 0], [0, 1, 0, 0], [0, 0, 0, 1]])
        linear_extrude(height = length) children();
}

module rounded_rectangle(size, radius) {
    translate([radius, radius])
        offset(r = radius) square(size - [2 * radius, 2 * radius]);
}

module outer_profile() {
    polygon([for (i = [0 : 5]) each corner_points(i)]);
}

module case_volume(inset = 0) {
    intersection() {
        translate([inset, 0, 0])
            extrude_along_x(case_width - 2 * inset)
                offset(delta = -inset) outer_profile();
        translate([inset, inset, -eps])
            linear_extrude(height = rear_height + 2 * eps)
                rounded_rectangle([case_width, case_depth] - [2 * inset, 2 * inset],
                                  bottom_radius - inset);
        // The upper corners of this mask lie above the case; only the base is rounded.
        translate([inset, -eps, inset])
            extrude_along_y(case_depth + 2 * eps)
                rounded_rectangle([case_width - 2 * inset,
                                   rear_height + 2 * bottom_radius - 2 * inset],
                                  bottom_radius - inset);
    }
}

module lid_profile(depth = panel_thickness, trim = joint_clearance) {
    path = top_path(trim);
    polygon(concat(path, reverse_list(inner_path(path, depth))));
}

module lid_opening_profile() {
    path = top_path();
    // Cross the exterior surface instead of leaving coincident CSG faces on the roof.
    polygon(concat(inner_path(path, -wall),
                   reverse_list(inner_path(path, panel_thickness + joint_clearance))));
}

module on_slope(x, along, normal = 0) {
    translate([x, deck_depth + cos(slope_angle) * along,
               deck_height + sin(slope_angle) * along])
        rotate([slope_angle, 0, 0])
            translate([0, 0, normal]) children();
}

module hole_pattern(center, spacing) {
    for (dx = [-spacing[0] / 2, spacing[0] / 2])
        for (dy = [-spacing[1] / 2, spacing[1] / 2])
            let($hole_x = center[0] + dx, $hole_y = center[1] + dy)
                translate([$hole_x, $hole_y, 0]) children();
}

module control_holes() {
    let($control_z = joystick_mount_z)
        hole_pattern(joystick_center, joystick_hole_spacing) children();
    let($control_z = keyboard_mount_z)
        hole_pattern(keyboard_center, keyboard_hole_spacing) children();
}

// The opening is at Z=0; screw and insert extend into negative Z.
module insert_socket() {
    translate([0, 0, -insert_depth])
        cylinder(d = insert_diameter, h = insert_depth + eps);
    translate([0, 0, -screw_tip_depth])
        cylinder(d = screw_clearance, h = screw_tip_depth + eps);
}

module shell_fasteners() {
    for (x = shell_hole_x)
        for (y = shell_hole_y)
            translate([x, y, (y < deck_depth ? deck_height : rear_height) - panel_thickness])
                children();
}

module shell_mount_columns() {
    for (x = shell_hole_x)
        for (y = shell_hole_y) {
            top = (y < deck_depth ? deck_height : rear_height) - panel_thickness;
            bounds = column_bounds(x, y);
            translate([bounds[0], bounds[1], floor_thickness - eps])
                cube([bounds[2] - bounds[0], bounds[3] - bounds[1],
                      top - floor_thickness + eps]);
        }
}

module control_mount_footprint() {
    radius = module_boss_diameter / 2;
    circle(d = module_boss_diameter);
    if ($hole_x - radius - wall <= small_gap_fill)
        translate([-$hole_x, -radius]) square([$hole_x, 2 * radius]);
    if (case_width - wall - $hole_x - radius <= small_gap_fill)
        translate([0, -radius]) square([case_width - $hole_x, 2 * radius]);
    if ($hole_y - radius - wall <= small_gap_fill)
        translate([-radius, -$hole_y]) square([2 * radius, $hole_y]);
    if (case_depth - wall - $hole_y - radius <= small_gap_fill)
        translate([-radius, 0]) square([2 * radius, case_depth - $hole_y]);
    for (x = shell_hole_x)
        for (y = shell_hole_y) {
            bounds = column_bounds(x, y);
            if (distance_to_rectangle([$hole_x, $hole_y], bounds) - radius <= small_gap_fill)
                hull() {
                    circle(d = module_boss_diameter);
                    translate([bounds[0] - $hole_x, bounds[1] - $hole_y])
                        square([bounds[2] - bounds[0], bounds[3] - bounds[1]]);
                }
        }
}

module screen_tab_pad(side, x, height) {
    hull() {
        translate([x, 0, 0]) cylinder(d = module_boss_diameter, h = height);
        translate([side < 0 ? wall / 2 : case_width - wall / 2, 0, 0])
            cylinder(d = wall, h = height);
    }
}

module screen_tabs() {
    for (side = [-1, 1])
        for (v = [-tft_hole_spacing[1] / 2, tft_hole_spacing[1] / 2]) {
            x = tft_center[0] + side * tft_hole_spacing[0] / 2;
            along = tft_center[1] + v;
            base_normal = screen_mount_normal - screen_tab_depth;
            reach = (side < 0 ? x : case_width - x) + module_boss_diameter / 2 - wall;
            base_y = deck_depth + cos(slope_angle) * along - sin(slope_angle) * base_normal;
            base_z = deck_height + sin(slope_angle) * along + cos(slope_angle) * base_normal;
            front_y = deck_depth + cos(slope_angle) * along -
                      sin(slope_angle) * screen_mount_normal -
                      module_boss_diameter / 2 * cos(slope_angle);
            back_y = base_y + module_boss_diameter / 2 * cos(slope_angle);
            root_z = base_z - module_boss_diameter / 2 * sin(slope_angle) -
                     reach - screen_gusset_thickness;
            // Cover the entire WORLD-Y projection, including directly below the hole.
            // The root drops at least as far as the cantilever reaches inward.
            hull() {
                on_slope(0, along, base_normal)
                    screen_tab_pad(side, x, screen_tab_depth);
                translate([side < 0 ? wall / 2 : case_width - 1.5 * wall,
                           front_y, root_z])
                    cube([wall, back_y - front_y, screen_gusset_thickness]);
            }
        }
}

module bottom_shell() {
    difference() {
        intersection() {
            case_volume();
            union() {
                difference() {
                    case_volume();
                    intersection() {
                        case_volume(wall);
                        translate([-eps, -eps, floor_thickness])
                            cube([case_width + 2 * eps, case_depth + 2 * eps, rear_height]);
                    }
                    translate([wall, 0, 0])
                        extrude_along_x(case_width - 2 * wall)
                            lid_opening_profile();
                }
                control_holes()
                    translate([0, 0, floor_thickness - eps])
                        linear_extrude(height = $control_z - floor_thickness + eps)
                            control_mount_footprint();
                screen_tabs();
                shell_mount_columns();
            }
        }
        control_holes() translate([0, 0, $control_z]) insert_socket();
        for (dx = [-tft_hole_spacing[0] / 2, tft_hole_spacing[0] / 2])
            for (dv = [-tft_hole_spacing[1] / 2, tft_hole_spacing[1] / 2])
                on_slope(tft_center[0] + dx, tft_center[1] + dv, screen_mount_normal)
                    insert_socket();
        shell_fasteners() insert_socket();
        if (enable_cable_opening)
            translate([(case_width - cable_opening[0]) / 2,
                       case_depth - wall - eps, cable_opening_z])
                cube([cable_opening[0], wall + 2 * eps, cable_opening[1]]);
    }
}

module lid() {
    difference() {
        translate([wall + joint_clearance, 0, 0])
            extrude_along_x(case_width - 2 * (wall + joint_clearance)) lid_profile();
        for (spec = [[joystick_center, joystick_window], [keyboard_center, keyboard_window]])
            translate([spec[0][0] - spec[1][0] / 2, spec[0][1] - spec[1][1] / 2,
                       deck_height - panel_thickness - eps])
                cube([spec[1][0], spec[1][1], panel_thickness + 2 * eps]);
        on_slope(tft_center[0] + screen_window_offset[0] -
                     (tft_view[0] + tft_window_clearance) / 2,
                 tft_center[1] + screen_window_offset[1] -
                     (tft_view[1] + tft_window_clearance) / 2,
                 -panel_thickness - eps)
            cube([tft_view[0] + tft_window_clearance,
                  tft_view[1] + tft_window_clearance, panel_thickness + 2 * eps]);
        shell_fasteners()
            translate([0, 0, -eps])
                cylinder(d = lid_screw_diameter, h = panel_thickness + 2 * eps);
    }
}

module module_boards(inset = 0) {
    assert(inset >= 0 && 2 * inset < pcb_thickness, "Invalid PCB inspection inset.");
    for (spec = [[joystick_center, joystick_size, joystick_mount_z],
                 [keyboard_center, keyboard_size, keyboard_mount_z]])
        translate([spec[0][0] - spec[1][0] / 2 + inset,
                   spec[0][1] - spec[1][1] / 2 + inset, spec[2] + inset])
            cube([spec[1][0] - 2 * inset, spec[1][1] - 2 * inset, pcb_thickness - 2 * inset]);
    on_slope(tft_center[0] - tft_size[0] / 2 + inset,
             tft_center[1] - tft_size[1] / 2 + inset, screen_mount_normal + inset)
        cube([tft_size[0] - 2 * inset, tft_size[1] - 2 * inset, pcb_thickness - 2 * inset]);
}

module control_envelope(center, size, mount_z, active_size, component_height, inset = 0) {
    translate([center[0] - size[0] / 2 + inset, center[1] - size[1] / 2 + inset,
               mount_z + pcb_thickness + inset])
        cube([size[0] - 2 * inset, size[1] - 2 * inset, component_height - 2 * inset]);
    translate([center[0] - active_size[0] / 2 + inset,
               center[1] - active_size[1] / 2 + inset, mount_z + pcb_thickness + inset])
        cube([active_size[0] - 2 * inset, active_size[1] - 2 * inset,
              size[2] - pcb_thickness - 2 * inset]);
}

module module_envelopes(inset = 0) {
    module_boards(inset);
    control_envelope(keyboard_center, keyboard_size, keyboard_mount_z,
                     keyboard_active_size, keyboard_component_height, inset);
    control_envelope(joystick_center, concat(joystick_size, [joystick_total_height]),
                     joystick_mount_z, joystick_active_size, joystick_component_height, inset);
    on_slope(tft_center[0] - tft_size[0] / 2 + inset,
             tft_center[1] - tft_size[1] / 2 + inset,
             screen_mount_normal + pcb_thickness + inset)
        cube([tft_size[0] - 2 * inset, tft_size[1] - 2 * inset,
              screen_stack_above_pcb - 2 * inset]);
    on_slope(tft_center[0] - tft_size[0] / 2 + screen_back_edge_margin + inset,
             tft_center[1] - tft_size[1] / 2 + screen_back_edge_margin + inset,
             screen_mount_normal - screen_back_height + inset)
        cube([tft_size[0] - 2 * (screen_back_edge_margin + inset),
              tft_size[1] - 2 * (screen_back_edge_margin + inset),
              screen_back_height - 2 * inset]);
}

module printable_lid() {
    translate([rear_height, -rim_radius - joint_clearance, -wall - joint_clearance])
        rotate([0, -90, 0]) lid();
}

if (part == "bottom") {
    bottom_shell();
} else if (part == "lid") {
    lid();
} else if (part == "lid-print") {
    printable_lid();
} else if (part == "assembly" || part == "exploded") {
    color([0.82, 0.82, 0.84]) bottom_shell();
    translate([0, 0, part == "exploded" ? explode_height : 0])
        color([0.95, 0.68, 0.2]) lid();
    if (show_modules) %module_envelopes();
}
