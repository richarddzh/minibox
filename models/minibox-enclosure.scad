/*
  Sketch-based enclosure. Units: mm.
  X = left/right, Y = front/back, Z = up.
  The front screen, rear quarter-arc cover and screen bracket print separately.
  Unmeasured radii, component heights and control windows are provisional.
*/

/* [View] */
part = "screen-bracket"; // [assembly, exploded, bottom, lid, rear-lid, screen-bracket, screen-bracket-print, lid-print, rear-lid-print, bottom-y-up, lid-y-up, rear-lid-y-up, screen-bracket-y-up]
show_modules = false;
explode_height = 35;
$fn = 24;

/* [Enclosure] */
case_width = 120;
deck_depth = 60;
deck_height = 32;
slope_angle = 90;
slope_length = 111;
wall = 2.4;
floor_thickness = 4;
panel_thickness = 3;
joint_clearance = 0.5;
lid_corner_radius = 6;
support_lip_width = 1.5;
support_lip_thickness = 1.5;

/* [Rounded corners] */
bottom_radius = 12;
rim_radius = 6;
bend_radius = 6;

/* [M2.5 fasteners] */
screw_clearance = 2.8;
lid_screw_diameter = 3;
rear_lid_screw_diameter = 3.2;
lid_counterbore_diameter = 4.5;
lid_counterbore_depth = 1.4;
lid_min_edge = 2;
screw_length = 10;
insert_diameter = 4.7;
insert_depth = 5.2;
screw_tip_depth = 8.8;
module_boss_diameter = 8;
shell_boss_diameter = 9;
shell_corner_depth = 7;
rear_mount_depth = 10;
middle_boss_diameter = 7;
middle_mount_y = 49.2;
small_gap_fill = 5;

/* [Controls: joystick left, keyboard right] */
joystick_size = [30, 40];
joystick_hole_spacing = [21, 28];
joystick_center = [31, 29];
joystick_hole_bottom_margin = 4.5;
joystick_hole_top_margin = 7.5;
joystick_window_diameter = 29;
joystick_window_from_left = 13.8;
joystick_window_from_bottom = 14.9;
joystick_lid_gap = 5;
joystick_total_height = 30;
keyboard_size = [57.4, 27.2, 20.6];
keyboard_hole_spacing = [53.34, 22.86];
keyboard_center = [84, 30.1];
keyboard_window = [57.4, 20.2];
keyboard_window_top_margin = 4;
keyboard_window_bottom_margin = 3;
keyboard_lid_gap = 5;
pcb_thickness = 1.6;
module_clearance = 1;
control_clearance = 2;

/* [ESP32 expansion board: rotated 90 degrees on the rear floor] */
esp32_size = [58, 68];
esp32_hole_spacing = [49, 58];
esp32_hole_diameter = 3;
esp32_standoff_height = 5;
esp32_rear_clearance = 20;
esp32_screw_length = 6;
esp32_socket_depth = 5.2;
// Provisional component height above the PCB; measure connectors and the ESP32 stack.
esp32_component_height = 10;

/* [Breadboard: beside ESP32, with aligned front edges] */
breadboard_size = [35, 47];
floor_board_gap = 10;
floor_module_clearance = 10;
floor_side_clearance = 1; // Minimum at the inward-curving bottom wall, not the straight wall
// Provisional thickness; no fixing holes or adhesive thickness have been supplied.
breadboard_height = 10;

/* [Landscape TFT: dimensions rotated together] */
tft_size = [108.04, 61.74];
tft_view = [83.52, 55.68];
tft_hole_spacing = [101.9, 54.9];
tft_view_side_margins = [10, 14.52]; // Left and right PCB edges to the active display
tft_window_clearance = 2; // Total enlargement: 1 mm per side
tft_window_radius = 6;
screen_pcb_thickness = 2;
screen_stack_above_pcb = 3;
screen_front_clearance = 1; // Reserved for acrylic film and assembly clearance
screen_back_height = 4;
screen_back_edge_margin = 8;
screen_shift_along = 26.62;
screen_port_clearance = 8;
screen_bracket_thickness = 3;
screen_bracket_bottom = 25;
screen_bracket_top = 135;
screen_bracket_front_y = 78;
screen_bracket_side_width = 13;
screen_bracket_right_width = 14;
screen_bracket_top_width = 6;
screen_bracket_clearance = 0.3;
screen_bracket_rail_depth = 2.5;
screen_header_opening = [10, 43];

/* [Switch below the screen, right side] */
switch_hole_diameter = 21;
switch_center = [90, 22]; // X and distance along the inclined panel
// Provisional envelope inside the case, not measured switch dimensions.
switch_body_depth = 20;
switch_body_diameter = 21;

/* [Audio openings left of the switch] */
speaker_diameter = 23;
speaker_hole_diameter = 23.5;
speaker_center = [30, 22];
microphone_hole_diameter = 5;
microphone_center = [60, 22];
// Provisional internal depths; mounting hardware and wiring are not yet measured.
speaker_body_depth = 10;
microphone_body_depth = 10;

/* [Rear panel Type-C sockets] */
typec_hole_diameter = 12;
typec_hole_spacing = 20;
typec_thread_diameter = 10.5;
typec_thread_length = 14.5;
typec_head_diameter = 13.8;
typec_head_thickness = 2;
typec_nut_outer_size = 14;
typec_nut_thickness = 3;

/* [Optional cable opening: absent in sketch] */
enable_cable_opening = false;
cable_opening = [16, 10];
cable_opening_z = 10;

/* [Hidden] */
eps = 0.02;
screen_top_y = deck_depth;
rear_height = deck_height + sin(slope_angle) * slope_length;
rear_arc_radius = slope_length;
screen_bracket_left = 4.5;
screen_bracket_right = case_width - 4.5;
rear_seam_angle = 20;
rear_seam_gap = 0.5;
rear_hole_stagger = 0;
rear_seam_y = deck_depth + rear_arc_radius * sin(rear_seam_angle);
typec_center_z = 21;
// Conservative envelope if the quoted hex size is across flats rather than corners.
typec_nut_envelope = typec_nut_outer_size / cos(30);
screen_board_gap = screen_stack_above_pcb + screen_front_clearance;
screen_mount_gap = screen_pcb_thickness + screen_board_gap;
screen_mount_normal = -panel_thickness - screen_mount_gap;
screen_window_offset = [(tft_view_side_margins[0] - tft_view_side_margins[1]) / 2, 0];
tft_center = [case_width / 2, 46 + screen_shift_along];
tft_rear_extent = deck_depth - screen_mount_normal;
floor_side_inset = (case_width - esp32_size[0] - floor_board_gap - breadboard_size[0]) / 2;
// Above-floor parts are checked in 3D, rather than wasting their entire XY projection.
esp32_front_y = max(keyboard_center[1] + keyboard_size[1] / 2,
                   joystick_center[1] + joystick_size[1] / 2,
                   middle_mount_y + middle_boss_diameter / 2) + floor_module_clearance;
esp32_center = [floor_side_inset + esp32_size[0] / 2,
                esp32_front_y + esp32_size[1] / 2];
breadboard_center = [case_width - floor_side_inset - breadboard_size[0] / 2,
                    esp32_front_y + breadboard_size[1] / 2];
breadboard_mount_z = floor_thickness;
floor_group_rear = esp32_front_y + max(esp32_size[1], breadboard_size[1]);
esp32_mount_z = floor_thickness + esp32_standoff_height;
case_depth = ceil(max(screen_top_y + rear_arc_radius,
                      tft_rear_extent + 10 + shell_boss_diameter / 2 + module_clearance,
                      floor_group_rear + esp32_rear_clearance + wall));
keyboard_window_offset = [0, (keyboard_window_bottom_margin - keyboard_window_top_margin) / 2];
keyboard_window_center = keyboard_center + keyboard_window_offset;
keyboard_mount_z = deck_height - panel_thickness - keyboard_lid_gap;
keyboard_standoff_height = keyboard_mount_z - floor_thickness;
// Peripheral components remain provisional; the measured opening bounds the tall keys.
keyboard_component_height = keyboard_lid_gap - pcb_thickness - module_clearance;
joystick_hole_center = joystick_center +
    [0, (joystick_hole_bottom_margin - joystick_hole_top_margin) / 2];
joystick_window_center = joystick_center - joystick_size / 2 +
    [joystick_window_from_left, joystick_window_from_bottom];
joystick_mount_z = deck_height - panel_thickness - joystick_lid_gap;
joystick_standoff_height = joystick_mount_z - floor_thickness;
joystick_component_height = joystick_lid_gap - pcb_thickness - module_clearance;
shell_hole_x = [9, case_width - 9];
shell_hole_y = [11, 114, case_depth - 32];
shell_head_diameter = lid_counterbore_diameter;
profile_vertices = [
    [0, 0], [case_depth, 0], [case_depth, deck_height],
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
    y < deck_depth ? 0 : y - shell_corner_depth / 2,
    x < case_width / 2 ? x + shell_boss_diameter / 2 : case_width,
    y < deck_depth ? y + shell_corner_depth / 2 : case_depth
];
function column_root_z(x, y) =
    y > deck_depth ?
        rear_arc_z(y) - panel_thickness - rear_mount_depth :
        floor_thickness;
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
function rear_arc_z(y) = deck_height +
    sqrt(rear_arc_radius * rear_arc_radius - (y - deck_depth) * (y - deck_depth));
function rear_arc_angle(y) = asin((y - deck_depth) / rear_arc_radius);
function rear_hole_y(x, y) = y + (x < case_width / 2 ? -rear_hole_stagger : rear_hole_stagger);
function rear_arc_points() = [
    for (i = [0 : 24])
        let(angle = 90 * i / 24)
            [round((deck_depth + rear_arc_radius * sin(angle)) * 1000000) / 1000000,
             round((deck_height + rear_arc_radius * cos(angle)) * 1000000) / 1000000]
];

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
        steps = max(4, ceil(abs(sweep) * $fn / 360)))
    // Remove trig drift at tangent endpoints before boolean intersections.
    [for (j = [0 : steps])
        [for (v = center + radius * [cos(a0 + sweep * j / steps), sin(a0 + sweep * j / steps)])
            round(v * 1000000) / 1000000]];

function top_path(trim = 0) = concat(
    [[rim_radius + trim, deck_height]],
    reverse_list(corner_points(4)),
    rear_arc_points()
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

// All six screws are vertical and sit on the two horizontal portions of the lid.
assert(part == "assembly" || part == "exploded" || part == "bottom" ||
       part == "lid" || part == "rear-lid" ||
       part == "lid-print" || part == "rear-lid-print" ||
       part == "bottom-y-up" || part == "lid-y-up" ||
       part == "rear-lid-y-up" || part == "screen-bracket" ||
       part == "screen-bracket-print" ||
       part == "screen-bracket-y-up" || part == "none", "Unknown part.");
assert(slope_angle == 90, "The screen panel must be vertical.");
assert(min(bottom_radius, rim_radius) > wall && bend_radius > panel_thickness,
       "Corner radii must exceed their wall/panel thickness.");
assert(floor_thickness >= wall, "Floor thickness must be at least the wall thickness.");
assert(joint_clearance > 0 && joint_clearance < wall, "Invalid lid clearance.");
assert(switch_center[0] > tft_center[0] &&
       switch_center[1] - switch_hole_diameter / 2 >= tangent_length(4) + lid_min_edge,
       "Switch must be on the right and above the lower curved transition.");
assert(screen_port_clearance >= 8, "Screen-to-port edge clearance must be at least 8 mm.");
assert(min(tft_view_side_margins) >= 0 &&
       abs(tft_view_side_margins[0] + tft_view[0] + tft_view_side_margins[1] -
           tft_size[0]) < 0.001, "Display width and side margins must match the PCB.");
assert(tft_window_clearance >= 2, "Screen aperture needs at least 1 mm per side.");
assert(tft_window_radius > 0 &&
       2 * tft_window_radius <= min(tft_view) + tft_window_clearance,
       "Screen window corner radius does not fit its aperture.");
assert(switch_center[1] + max(switch_hole_diameter, switch_body_diameter) / 2 +
       screen_port_clearance <= tft_center[1] - tft_size[1] / 2,
       "Switch must clear the entire screen PCB, not just the display window.");
assert(tft_center[1] + tft_size[1] / 2 + 5 < slope_length,
       "Screen overlaps the upper edge.");
assert(switch_body_depth > 0 && switch_body_diameter > 0,
       "Switch envelope dimensions must be positive.");
assert(speaker_center[0] < microphone_center[0] && microphone_center[0] < switch_center[0],
       "Audio openings must remain to the left of the switch.");
for (port = [[speaker_center, speaker_hole_diameter],
             [microphone_center, microphone_hole_diameter]])
    assert(port[0][1] - port[1] / 2 >= tangent_length(4) + lid_min_edge &&
           port[0][1] + port[1] / 2 + screen_port_clearance <= tft_center[1] - tft_size[1] / 2,
           "Audio opening must clear both the bend and the screen PCB.");
assert(norm(speaker_center - microphone_center) -
       (speaker_hole_diameter + microphone_hole_diameter) / 2 >= lid_min_edge &&
       norm(microphone_center - switch_center) -
       (microphone_hole_diameter + switch_hole_diameter) / 2 >= lid_min_edge,
       "Audio and switch openings need at least 2 mm webs.");
assert(typec_hole_diameter > typec_thread_diameter &&
       typec_head_diameter > typec_hole_diameter,
       "Type-C thread needs clearance and the flange must overlap the panel hole.");
assert(typec_thread_length > wall + typec_nut_thickness &&
       typec_hole_spacing > max(typec_head_diameter, typec_nut_envelope),
       "Type-C sockets need room for both nuts and thread engagement.");
assert(typec_center_z - typec_nut_envelope / 2 > bottom_radius &&
       typec_center_z + typec_nut_envelope / 2 < deck_height,
       "Type-C fittings must remain on the flat rear wall.");
assert(lid_corner_radius > joint_clearance && support_lip_width > joint_clearance,
       "The rounded opening and support lip must retain lid clearance and bearing overlap.");
assert(support_lip_thickness >= 1.5, "Support lip needs sufficient printing thickness.");
assert(lid_counterbore_diameter > lid_screw_diameter &&
       lid_counterbore_depth > 0 && panel_thickness - lid_counterbore_depth >= 1.6,
       "Counterbores must leave at least 1.6 mm of lid under the screw head.");
assert(shell_corner_depth >= insert_diameter + 2,
       "Corner screw seats need at least 1 mm around the glue sockets.");
assert(middle_mount_y > max(keyboard_center[1] + keyboard_size[1] / 2,
                           joystick_center[1] + joystick_size[1] / 2),
       "Middle lid screws must be behind both control modules.");
assert(middle_mount_y + shell_head_diameter / 2 + 0.5 <
       deck_depth - tangent_length(4),
       "Middle screws and head clearance must stay entirely on the horizontal deck.");
assert(middle_boss_diameter >= insert_diameter + 2,
       "Middle screw seats need at least 1 mm material around the nut pocket.");
assert(floor_module_clearance >= 10 && floor_board_gap == 10,
       "Floor modules need 10 mm to other parts and exactly 10 mm between boards.");
assert(esp32_rear_clearance >= 20, "ESP32 needs at least 20 mm to the rear panel.");
assert(floor_side_clearance > 0 && floor_side_inset >= wall + floor_side_clearance,
       "Floor modules must clear both side walls.");
assert(breadboard_center[0] > esp32_center[0] &&
       abs(breadboard_center[0] - breadboard_size[0] / 2 -
           esp32_center[0] - esp32_size[0] / 2 - floor_board_gap) < eps,
       "Place the breadboard beside ESP32 with a 10 mm edge gap.");
assert(rear_mount_depth >= max(insert_depth, screw_tip_depth) + 1.2,
       "Rear seats must retain at least 1.2 mm below the nut and screw-tip pockets.");
assert(min(breadboard_size) > 0 && breadboard_height > 0,
       "Breadboard dimensions must be positive.");
assert(esp32_mount_z - max(insert_depth, esp32_socket_depth) >= 1.5,
       "ESP32 blind sockets must leave at least 1.5 mm of solid floor.");
assert(esp32_screw_length > pcb_thickness &&
       esp32_screw_length - pcb_thickness + 0.2 <= esp32_socket_depth,
       "ESP32 screws are too long for the 5 mm posts; use shorter screws.");
assert(esp32_component_height > 0 && esp32_standoff_height > 0,
       "ESP32 mount and component heights must be positive.");
for (axis = [0, 1])
    assert(esp32_hole_spacing[axis] > 0 &&
           esp32_hole_spacing[axis] + esp32_hole_diameter < esp32_size[axis],
           "ESP32 mounting holes must stay inside the PCB.");
assert(joystick_standoff_height >= insert_depth + 1,
       "Joystick posts need material below the glued nut pockets.");
assert(joystick_component_height > 0, "Joystick gap must accommodate the PCB and clearance.");
assert(abs(joystick_hole_spacing[1] + joystick_hole_bottom_margin +
           joystick_hole_top_margin - joystick_size[1]) < 0.001,
       "Joystick hole spacing and edge margins must match the module.");
assert(joystick_window_diameter > 0 &&
       joystick_window_center[0] - joystick_window_diameter / 2 > wall + joint_clearance &&
       joystick_window_center[1] - joystick_window_diameter / 2 > rim_radius + joint_clearance &&
       joystick_window_center[1] + joystick_window_diameter / 2 < deck_depth - tangent_length(4),
       "Joystick circular opening overlaps the lid boundary or bend.");
assert(keyboard_standoff_height >= insert_depth + 1, "Keyboard posts are too short for the nuts.");
assert(keyboard_component_height > 0, "Keyboard gap must accommodate the PCB and clearance.");
assert(abs(keyboard_window[0] - keyboard_size[0]) < 0.001 &&
       abs(keyboard_window[1] + keyboard_window_top_margin +
           keyboard_window_bottom_margin - keyboard_size[1]) < 0.001,
       "Keyboard opening and edge margins must match the module dimensions.");
assert(min(keyboard_mount_z, joystick_mount_z) - screw_tip_depth >= 1.5,
       "Thicken the floor: the 10 mm screws must leave at least 1.5 mm below the tip pockets.");
assert(screen_front_clearance >= module_clearance, "Insufficient screen-to-lid clearance.");
assert(screen_pcb_thickness > 0 && screen_stack_above_pcb > 0,
       "Screen PCB and display thicknesses must be positive.");
assert(keyboard_mount_z + pcb_thickness + keyboard_component_height + module_clearance <=
       deck_height - panel_thickness, "Keyboard body hits the lid.");
assert(joystick_mount_z + pcb_thickness + joystick_component_height + module_clearance <=
       deck_height - panel_thickness,
       "Joystick body hits the lid.");
assert(keyboard_center[0] - keyboard_size[0] / 2 -
       joystick_center[0] - joystick_size[0] / 2 >= control_clearance,
       "Keyboard and joystick modules overlap.");
assert(joystick_center[0] - joystick_size[0] / 2 >=
       shell_hole_x[0] + shell_boss_diameter / 2 + control_clearance &&
       keyboard_center[1] - keyboard_size[1] / 2 >=
       shell_hole_y[0] + shell_corner_depth / 2 + control_clearance,
       "Controls are too close to the front lid columns.");
assert(screen_bracket_thickness > 0 && screen_bracket_clearance > 0 &&
       screen_bracket_front_y - tft_rear_extent >= screen_back_height + 0.5,
       "The bracket must clear the rear of the screen.");
assert(screen_bracket_bottom > esp32_mount_z + pcb_thickness + esp32_component_height &&
       screen_bracket_top < rear_arc_z(screen_bracket_front_y + screen_bracket_thickness) -
                            panel_thickness,
       "Bracket rails must clear the floor boards and curved lid.");
assert(screen_bracket_right_width - screen_header_opening[0] >= 4 &&
       screen_bracket_top_width >= 6 && screen_bracket_thickness >= 3,
       "Bracket frame needs substantial printable webs around the cable opening.");
assert(screen_bracket_left > wall + support_lip_width + screen_bracket_clearance &&
       screen_bracket_right < case_width - wall - support_lip_width -
                              screen_bracket_clearance,
       "The complete bracket must pass vertically through the open lid seat.");
assert(screw_length - pcb_thickness <= screw_tip_depth &&
       screw_length - screen_pcb_thickness <= screw_tip_depth &&
       screw_length - (panel_thickness - lid_counterbore_depth) <= screw_tip_depth,
       "Screws are too long for the blind holes.");
assert(insert_depth < screw_tip_depth && insert_diameter < module_boss_diameter - 2,
       "Insufficient insert depth or boss wall.");
assert(tft_size[0] + 2 * (wall + joint_clearance) < case_width,
       "Landscape screen does not fit between the side walls.");
assert(tft_size[1] / 2 + tft_center[1] + 5 < slope_length,
       "TFT overlaps the upper curved transition.");
assert(case_depth == deck_depth + rear_arc_radius,
       "The back must be a complete quarter-circle.");
assert(rear_seam_angle > 10 && rear_seam_angle < 35 &&
       shell_hole_y[1] + shell_boss_diameter / 2 < shell_hole_y[2] - shell_boss_diameter / 2 &&
       rear_seam_gap > 0 && rear_seam_gap < support_lip_width,
       "Rear cover split and screw seats must have room.");
for (spec = [[joystick_center, joystick_size], [keyboard_center, keyboard_size]])
    assert(spec[0][0] - spec[1][0] / 2 > wall &&
           spec[0][0] + spec[1][0] / 2 < case_width - wall &&
           spec[0][1] - spec[1][1] / 2 > rim_radius &&
           spec[0][1] + spec[1][1] / 2 < deck_depth - tangent_length(4),
           "Control PCB overlaps a wall or the lower curved transition.");

module extrude_along_x(length) {
    multmatrix([[0, 0, 1, 0], [1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 0, 1]])
        linear_extrude(height = length, convexity = 10) children();
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
    // A single sampled arc joins the upright screen face to the rear vertical wall.
    polygon(concat([[0, -bottom_radius], [case_depth, -bottom_radius]],
                   reverse_list(rear_arc_points()),
                   corner_points(4), corner_points(5)));
}

module corner_sphere(radius) {
    segments = max(8, $fn);
    latitude_steps = max(4, 2 * ceil(segments / 4));
    top = 1 + (latitude_steps - 1) * segments;
    // Explicit poles and equator preserve Z=0 and tangent edges even at $fn=16.
    polyhedron(
        points = concat(
            [[0, 0, -radius]],
            [for (i = [1 : latitude_steps - 1], j = [0 : segments - 1])
                [radius * sin(180 * i / latitude_steps) * cos(360 * j / segments),
                 radius * sin(180 * i / latitude_steps) * sin(360 * j / segments),
                 -radius * cos(180 * i / latitude_steps)]],
            [[0, 0, radius]]
        ),
        faces = concat(
            [for (j = [0 : segments - 1]) [0, 1 + (j + 1) % segments, 1 + j]],
            [for (i = [0 : latitude_steps - 3], j = [0 : segments - 1]) each
                [[1 + i * segments + j,
                  1 + i * segments + (j + 1) % segments,
                  1 + (i + 1) * segments + (j + 1) % segments],
                 [1 + i * segments + j,
                  1 + (i + 1) * segments + (j + 1) % segments,
                  1 + (i + 1) * segments + j]]],
            [for (j = [0 : segments - 1])
                [top, top - segments + j, top - segments + (j + 1) % segments]]
        ),
        convexity = 2
    );
}

module spherical_base_volume(inset = 0) {
    radius = bottom_radius - inset;
    // One hull shares the sphere equator with the vertical wall. Separately
    // rounded extrusions create near-coincident seams at Z=bottom_radius.
    hull()
        for (x = [bottom_radius, case_width - bottom_radius])
            for (y = [bottom_radius, case_depth - bottom_radius])
                for (z = [bottom_radius, rear_height + wall])
                    translate([x, y, z]) corner_sphere(radius);
}

module case_volume(inset = 0) {
    intersection() {
        translate([inset, 0, 0])
            extrude_along_x(case_width - 2 * inset)
                offset(delta = -inset) outer_profile();
        spherical_base_volume(inset);
    }
}

module lid_profile(depth = panel_thickness, trim = joint_clearance) {
    path = top_path(trim);
    polygon(concat(path, reverse_list(inner_path(path, depth))));
}

module panel_band(start_depth, end_depth) {
    path = top_path(-rim_radius);
    polygon(concat(inner_path(path, start_depth),
                   reverse_list(inner_path(path, end_depth))));
}

module lid_plan() {
    translate([wall + joint_clearance, rim_radius + joint_clearance])
        rounded_rectangle([case_width - 2 * (wall + joint_clearance),
                           case_depth - 2 * (rim_radius + joint_clearance)],
                          lid_corner_radius);
}

// Positive offsets share the same corner centers; the mating arc is R6.5.
module opening_plan(inset = 0) {
    offset(delta = -inset)
        offset(r = joint_clearance) lid_plan();
}

module plan_volume() {
    translate([0, 0, -eps])
        linear_extrude(height = rear_height + 2 * eps, convexity = 10) children();
}

module lid_blank() {
    intersection() {
        extrude_along_x(case_width) panel_band(0, panel_thickness);
        plan_volume() lid_plan();
    }
}

module lid_opening_profile() {
    // Cross the exterior surface instead of leaving coincident CSG faces on the roof.
    panel_band(-wall, panel_thickness + eps);
}

module support_lip() {
    union() {
        intersection() {
            extrude_along_x(case_width)
                panel_band(panel_thickness, panel_thickness + support_lip_thickness);
            plan_volume()
                difference() {
                    opening_plan(-wall);
                    opening_plan(support_lip_width);
                }
        }
        // Fill the transition above the ledge outside the opening, including rounded corners.
        intersection() {
            extrude_along_x(case_width)
                panel_band(-wall, panel_thickness + support_lip_thickness);
            plan_volume()
                difference() {
                    square([case_width, case_depth]);
                    opening_plan();
                }
        }
        intersection() {
            extrude_along_x(case_width)
                panel_band(panel_thickness, panel_thickness + support_lip_thickness);
            translate([0, rear_seam_y - 5, 0])
                cube([case_width, 10, rear_height + eps]);
        }
    }
}

module middle_fasteners() {
    for (x = shell_hole_x)
        translate([x, middle_mount_y, deck_height - panel_thickness]) children();
}

module middle_mount_columns() {
    for (x = shell_hole_x)
        translate([x < case_width / 2 ? 0 : x - middle_boss_diameter / 2,
                   middle_mount_y - middle_boss_diameter / 2, floor_thickness - eps])
            cube([x < case_width / 2 ? x + middle_boss_diameter / 2 :
                      case_width - x + middle_boss_diameter / 2,
                  middle_boss_diameter,
                  deck_height - panel_thickness - floor_thickness + eps]);
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
        hole_pattern(joystick_hole_center, joystick_hole_spacing) children();
    let($control_z = keyboard_mount_z)
        hole_pattern(keyboard_center, keyboard_hole_spacing) children();
}

// The opening is at Z=0; screw and insert extend into negative Z.
module insert_socket(tip_depth = screw_tip_depth, extension = 0) {
    translate([0, 0, -insert_depth - extension])
        cylinder(d = insert_diameter, h = insert_depth + extension + eps);
    translate([0, 0, -tip_depth - extension])
        cylinder(d = screw_clearance, h = tip_depth + extension + eps);
}

module shell_fasteners(offset = 0) {
    front_fasteners(offset) children();
    for (x = shell_hole_x)
        for (y = [shell_hole_y[1], shell_hole_y[2]])
            on_rear(x, rear_hole_y(x, y), -panel_thickness + offset) children();
}

module front_fasteners(offset = 0) {
    for (x = shell_hole_x)
        translate([x, shell_hole_y[0], deck_height - panel_thickness + offset]) children();
    middle_fasteners() translate([0, 0, offset]) children();
}

module on_rear(x, y, normal = 0) {
    angle = rear_arc_angle(y);
    translate([x, y, rear_arc_z(y)])
        rotate([-angle, 0, 0])
            translate([0, 0, normal]) children();
}

module shell_mount_columns() {
    for (x = shell_hole_x)
        for (y = [shell_hole_y[0]]) {
            top = deck_height - panel_thickness;
            bounds = column_bounds(x, y);
            width = bounds[2] - bounds[0];
            translate([bounds[0], bounds[1], floor_thickness - eps])
                cube([width, bounds[3] - bounds[1], top - floor_thickness + eps]);
        }
    for (x = shell_hole_x)
        for (y = [shell_hole_y[1], shell_hole_y[2]])
            hull() {
                on_rear(x, rear_hole_y(x, y), -panel_thickness - rear_mount_depth)
                    cylinder(d = shell_boss_diameter, h = rear_mount_depth + eps);
                on_rear(x < case_width / 2 ? wall / 2 : case_width - wall / 2,
                        rear_hole_y(x, y), -panel_thickness - rear_mount_depth)
                    cylinder(d = wall, h = rear_mount_depth + eps);
            }
    middle_mount_columns();
}

module control_mount_footprint(top_z) {
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
            if (column_root_z(x, y) <= top_z &&
                distance_to_rectangle([$hole_x, $hole_y], bounds) - radius <= small_gap_fill)
                hull() {
                    circle(d = module_boss_diameter);
                    translate([bounds[0] - $hole_x + eps, bounds[1] - $hole_y + eps])
                        square([bounds[2] - bounds[0] - 2 * eps,
                                bounds[3] - bounds[1] - 2 * eps]);
                }
        }
}

module screen_bracket() {
    difference() {
        union() {
            translate([screen_bracket_left, screen_bracket_front_y, screen_bracket_bottom])
                cube([screen_bracket_side_width, screen_bracket_thickness,
                      screen_bracket_top - screen_bracket_bottom]);
            translate([screen_bracket_right - screen_bracket_right_width,
                       screen_bracket_front_y, screen_bracket_bottom])
                cube([screen_bracket_right_width, screen_bracket_thickness,
                      screen_bracket_top - screen_bracket_bottom]);
            translate([screen_bracket_left, screen_bracket_front_y,
                       screen_bracket_top - screen_bracket_top_width])
                cube([screen_bracket_right - screen_bracket_left,
                      screen_bracket_thickness,
                      screen_bracket_top_width]);
            for (dx = [-tft_hole_spacing[0] / 2, tft_hole_spacing[0] / 2])
                for (dv = [-tft_hole_spacing[1] / 2, tft_hole_spacing[1] / 2])
                    on_slope(tft_center[0] + dx, tft_center[1] + dv,
                             screen_mount_normal - screen_bracket_front_y - 0.4 +
                             tft_rear_extent)
                        cylinder(d = module_boss_diameter,
                                 h = screen_bracket_front_y - tft_rear_extent + 0.4);
        }
        for (dx = [-tft_hole_spacing[0] / 2, tft_hole_spacing[0] / 2])
            for (dv = [-tft_hole_spacing[1] / 2, tft_hole_spacing[1] / 2])
                on_slope(tft_center[0] + dx, tft_center[1] + dv, screen_mount_normal)
                    insert_socket();
        translate([screen_bracket_right - screen_bracket_right_width,
                   screen_bracket_front_y - eps,
                   deck_height + tft_center[1] - screen_header_opening[1] / 2])
            cube([screen_header_opening[0],
                  screen_bracket_thickness + 2 * eps, screen_header_opening[1]]);
    }
}

module screen_bracket_tracks() {
    rail_width = 6.1;
    rail_thickness = 1.5;
    header_bottom = deck_height + tft_center[1] - screen_header_opening[1] / 2;
    header_top = header_bottom + screen_header_opening[1];
    lower_hole = deck_height + tft_center[1] - tft_hole_spacing[1] / 2;
    for (x = [wall - eps, case_width - wall - rail_width]) {
        translate([x, screen_bracket_front_y - screen_bracket_clearance -
                       rail_thickness, screen_bracket_bottom - screen_bracket_rail_depth])
            cube([rail_width + eps,
                  screen_bracket_thickness + 2 * screen_bracket_clearance +
                  2 * rail_thickness, screen_bracket_rail_depth]);
        translate([x, screen_bracket_front_y - screen_bracket_clearance -
                   rail_thickness, screen_bracket_bottom - eps])
            cube([rail_width + eps, rail_thickness,
                  lower_hole - 6 - screen_bracket_bottom + eps]);
        for (range = x < case_width / 2 ?
             [[screen_bracket_bottom, screen_bracket_top]] :
             [[screen_bracket_bottom, header_bottom], [header_top, screen_bracket_top]])
            translate([x, screen_bracket_front_y + screen_bracket_thickness +
                       screen_bracket_clearance, range[0] - eps])
                cube([rail_width + eps, rail_thickness,
                      range[1] - range[0] + eps]);
    }
}

module rear_typec_positions() {
    for (dx = [-typec_hole_spacing / 2, typec_hole_spacing / 2])
        translate([case_width / 2 + dx, case_depth, typec_center_z])
            rotate([90, 0, 0]) children();
}

module typec_envelopes(inset = 0) {
    rear_typec_positions() {
        translate([0, 0, inset])
            cylinder(d = typec_thread_diameter - 2 * inset,
                     h = typec_thread_length - 2 * inset);
        translate([0, 0, -typec_head_thickness + inset])
            cylinder(d = typec_head_diameter - 2 * inset,
                     h = typec_head_thickness - 2 * inset);
        translate([0, 0, wall + inset])
            cylinder(d = typec_nut_envelope - 2 * inset,
                     h = typec_nut_thickness - 2 * inset);
    }
}

module switch_envelope(inset = 0) {
    on_slope(switch_center[0], switch_center[1],
             -panel_thickness - switch_body_depth + inset)
        cylinder(d = switch_body_diameter - 2 * inset,
                 h = switch_body_depth - 2 * inset);
}

module audio_envelopes(inset = 0) {
    for (port = [[speaker_center, speaker_diameter, speaker_body_depth],
                 [microphone_center, microphone_hole_diameter, microphone_body_depth]])
        on_slope(port[0][0], port[0][1], -panel_thickness - port[2] + inset)
            cylinder(d = port[1] - 2 * inset, h = port[2] - 2 * inset);
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
                    intersection() {
                        extrude_along_x(case_width) lid_opening_profile();
                        plan_volume() opening_plan();
                    }
                }
                support_lip();
                control_holes()
                    translate([0, 0, floor_thickness - eps])
                        linear_extrude(height = $control_z - floor_thickness + eps)
                            control_mount_footprint($control_z);
                hole_pattern(esp32_center, esp32_hole_spacing)
                    translate([0, 0, floor_thickness - eps])
                        linear_extrude(height = esp32_standoff_height + eps)
                            control_mount_footprint(esp32_mount_z);
                screen_bracket_tracks();
                shell_mount_columns();
            }
        }
        control_holes() translate([0, 0, $control_z]) insert_socket();
        hole_pattern(esp32_center, esp32_hole_spacing)
            translate([0, 0, esp32_mount_z]) insert_socket(esp32_socket_depth);
        shell_fasteners(0.6) insert_socket(screw_tip_depth, 0.6);
        rear_typec_positions()
            translate([0, 0, -eps])
                cylinder(d = typec_hole_diameter, h = wall + 2 * eps);
        if (enable_cable_opening)
            translate([(case_width - cable_opening[0]) / 2,
                       case_depth - wall - eps, cable_opening_z])
                cube([cable_opening[0], wall + 2 * eps, cable_opening[1]]);
    }
}

module lid_openings() {
        for (port = [[speaker_center, speaker_hole_diameter],
                     [microphone_center, microphone_hole_diameter]])
            on_slope(port[0][0], port[0][1], -panel_thickness - eps)
                cylinder(d = port[1], h = panel_thickness + 2 * eps);
        on_slope(switch_center[0], switch_center[1], -panel_thickness - eps)
            cylinder(d = switch_hole_diameter, h = panel_thickness + 2 * eps);
        translate([keyboard_window_center[0] - keyboard_window[0] / 2,
                   keyboard_window_center[1] - keyboard_window[1] / 2,
                   deck_height - panel_thickness - eps])
            cube([keyboard_window[0], keyboard_window[1], panel_thickness + 2 * eps]);
        translate([joystick_window_center[0], joystick_window_center[1],
                   deck_height - panel_thickness - eps])
            cylinder(d = joystick_window_diameter, h = panel_thickness + 2 * eps);
        on_slope(tft_center[0] + screen_window_offset[0] -
                     (tft_view[0] + tft_window_clearance) / 2,
                 tft_center[1] + screen_window_offset[1] -
                     (tft_view[1] + tft_window_clearance) / 2,
                 -panel_thickness - eps)
            linear_extrude(height = panel_thickness + 2 * eps)
                rounded_rectangle(tft_view + [tft_window_clearance, tft_window_clearance],
                                  tft_window_radius);
        front_fasteners()
            translate([0, 0, -eps])
                cylinder(d = lid_screw_diameter, h = panel_thickness + wall);
        for (x = shell_hole_x)
            for (y = [shell_hole_y[1], shell_hole_y[2]])
                on_rear(x, rear_hole_y(x, y), -panel_thickness - 1)
                    cylinder(d = rear_lid_screw_diameter,
                             h = panel_thickness + wall + 2);
        front_fasteners()
            translate([0, 0, panel_thickness - lid_counterbore_depth])
                cylinder(d = lid_counterbore_diameter, h = lid_counterbore_depth + eps);
}

module complete_lid() {
    difference() {
        lid_blank();
        lid_openings();
    }
}

module lid() {
    intersection() {
        complete_lid();
        translate([-eps, -eps, -eps])
            cube([case_width + 2 * eps, rear_seam_y - rear_seam_gap / 2 + eps,
                  rear_height + 2 * eps]);
    }
}

module rear_lid() {
    intersection() {
        complete_lid();
        translate([-eps, rear_seam_y + rear_seam_gap / 2, -eps])
            cube([case_width + 2 * eps, case_depth - rear_seam_y + eps,
                  rear_height + 2 * eps]);
    }
}

module module_boards(inset = 0, include_esp32 = true) {
    assert(inset >= 0 && 2 * inset < min(pcb_thickness, screen_pcb_thickness),
           "Invalid PCB inspection inset.");
    for (spec = [[joystick_center, joystick_size, joystick_mount_z],
                 [keyboard_center, keyboard_size, keyboard_mount_z]])
        translate([spec[0][0] - spec[1][0] / 2 + inset,
                   spec[0][1] - spec[1][1] / 2 + inset, spec[2] + inset])
            cube([spec[1][0] - 2 * inset, spec[1][1] - 2 * inset, pcb_thickness - 2 * inset]);
    on_slope(tft_center[0] - tft_size[0] / 2 + inset,
             tft_center[1] - tft_size[1] / 2 + inset, screen_mount_normal + inset)
        cube([tft_size[0] - 2 * inset, tft_size[1] - 2 * inset, screen_pcb_thickness - 2 * inset]);
    if (include_esp32) difference() {
        translate([esp32_center[0] - esp32_size[0] / 2 + inset,
                   esp32_front_y + inset, esp32_mount_z + inset])
            cube([esp32_size[0] - 2 * inset, esp32_size[1] - 2 * inset,
                  pcb_thickness - 2 * inset]);
        hole_pattern(esp32_center, esp32_hole_spacing)
            translate([0, 0, esp32_mount_z - eps])
                cylinder(d = esp32_hole_diameter, h = pcb_thickness + 2 * eps);
    }
}

module control_envelope(center, size, mount_z, active_size, component_height, inset = 0,
                        active_offset = [0, 0]) {
    translate([center[0] - size[0] / 2 + inset, center[1] - size[1] / 2 + inset,
               mount_z + pcb_thickness + inset])
        cube([size[0] - 2 * inset, size[1] - 2 * inset, component_height - 2 * inset]);
    translate([center[0] + active_offset[0] - active_size[0] / 2 + inset,
               center[1] + active_offset[1] - active_size[1] / 2 + inset,
               mount_z + pcb_thickness + inset])
        cube([active_size[0] - 2 * inset, active_size[1] - 2 * inset,
              size[2] - pcb_thickness - 2 * inset]);
}

module breadboard_envelope(inset = 0) {
    translate([breadboard_center[0] - breadboard_size[0] / 2 + inset,
               breadboard_center[1] - breadboard_size[1] / 2 + inset,
               breadboard_mount_z + inset])
        cube([breadboard_size[0] - 2 * inset, breadboard_size[1] - 2 * inset,
              breadboard_height - 2 * inset]);
}

module module_envelopes(inset = 0, include_floor_modules = true) {
    module_boards(inset, include_floor_modules);
    if (include_floor_modules) {
        breadboard_envelope(inset);
        translate([esp32_center[0] - esp32_size[0] / 2 + inset,
                   esp32_front_y + inset, esp32_mount_z + pcb_thickness + inset])
            cube([esp32_size[0] - 2 * inset, esp32_size[1] - 2 * inset,
                  esp32_component_height - 2 * inset]);
    }
    control_envelope(keyboard_center, keyboard_size, keyboard_mount_z,
                     keyboard_window, keyboard_component_height, inset, keyboard_window_offset);
    translate([joystick_center[0] - joystick_size[0] / 2 + inset,
               joystick_center[1] - joystick_size[1] / 2 + inset,
               joystick_mount_z + pcb_thickness + inset])
        cube([joystick_size[0] - 2 * inset, joystick_size[1] - 2 * inset,
              joystick_component_height - 2 * inset]);
    // The requested circular opening bounds the tall operating envelope, not the PCB.
    translate([joystick_window_center[0], joystick_window_center[1],
               joystick_mount_z + pcb_thickness + inset])
        cylinder(d = joystick_window_diameter - 2 * inset,
                 h = joystick_total_height - pcb_thickness - 2 * inset);
    on_slope(tft_center[0] - tft_size[0] / 2 + inset,
             tft_center[1] - tft_size[1] / 2 + inset,
             screen_mount_normal + screen_pcb_thickness + inset)
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

module printable_rear_lid() {
    translate([rear_height, -rim_radius - joint_clearance, -wall - joint_clearance])
        rotate([0, -90, 0]) rear_lid();
}

module printable_screen_bracket() {
    translate([0, -screen_bracket_bottom,
               screen_bracket_front_y + screen_bracket_thickness])
        rotate([-90, 0, 0]) screen_bracket();
}

if (part == "bottom") {
    bottom_shell();
} else if (part == "lid") {
    lid();
} else if (part == "rear-lid") {
    rear_lid();
} else if (part == "lid-print") {
    printable_lid();
} else if (part == "rear-lid-print") {
    printable_rear_lid();
} else if (part == "screen-bracket") {
    screen_bracket();
} else if (part == "screen-bracket-print") {
    printable_screen_bracket();
} else if (part == "bottom-y-up") {
    translate([0, 0, case_depth])
        rotate([-90, 0, 0]) bottom_shell();
} else if (part == "lid-y-up") {
    translate([0, 0, case_depth - 2 * (rim_radius + joint_clearance)])
        rotate([-90, 0, 0]) printable_lid();
} else if (part == "rear-lid-y-up") {
    translate([0, 0, case_depth - 2 * (rim_radius + joint_clearance)])
        rotate([-90, 0, 0]) printable_rear_lid();
} else if (part == "screen-bracket-y-up") {
    translate([0, 0, screen_bracket_front_y + screen_bracket_thickness])
        rotate([-90, 0, 0]) screen_bracket();
} else if (part == "assembly" || part == "exploded") {
    color([0.82, 0.82, 0.84]) bottom_shell();
    translate([0, 0, part == "exploded" ? explode_height : 0])
        color([0.95, 0.68, 0.2]) lid();
    translate([0, 0, part == "exploded" ? explode_height : 0])
        color([0.92, 0.6, 0.15]) rear_lid();
    translate([0, 0, part == "exploded" ? explode_height / 2 : 0])
        color([0.2, 0.6, 0.8]) screen_bracket();
    if (show_modules) {
        %module_envelopes();
        %typec_envelopes();
        %switch_envelope();
        %audio_envelopes();
    }
}
