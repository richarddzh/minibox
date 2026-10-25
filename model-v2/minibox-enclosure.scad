/*
  V2 carrier-board enclosure. Units: mm.
  X = left/right, Y = front/back, Z = up.
  Both halves use the same tangent, rounded side profile.
  Unmeasured radii, component heights and control windows are provisional.
*/

/* [View] */
part = "bottom"; // [assembly, exploded, bottom, lid, lid-print, bottom-y-up, lid-y-up]
show_modules = false;
explode_height = 35;
$fn = 32;

/* [Enclosure] */
case_width = 120;
case_depth = 136;
deck_depth = 55.8; // Riser exterior; its 3 mm inner wall stops at Y=58.8 before the amplifier
deck_height = 21.6; // 11 mm above the carrier PCB top
slope_base_height = 46;
slope_angle = 65; // 25 degrees back from vertical (v1: 10 degrees)
slope_length = 70.4; // Screen PCB top to the roof bend: 1.208 mm
wall = 2.4;
floor_thickness = 4;
panel_thickness = 3;
joint_clearance = 0.5;
lid_corner_radius = 6;
support_lip_width = 1.5;
support_lip_thickness = 1.5;
minimum_print_wall = 1.2;

/* [Rounded corners] */
bottom_radius = 12;
rim_radius = 6;
bend_radius = 6;
deck_bend_radius = 4;

/* [M2.5 fasteners] */
screw_clearance = 2.8;
lid_screw_diameter = 3;
lid_counterbore_diameter = 4.5;
lid_counterbore_depth = 1.4;
lid_min_edge = 2;
screw_length = 10;
insert_diameter = 4.7;
insert_depth = 5.2;
screw_tip_depth = 8.8;
module_boss_diameter = 8;
screen_tab_depth = 10;
shell_boss_diameter = 9;
shell_corner_depth = 7;
rear_mount_depth = 10;
screen_gusset_thickness = 2;
support_angle = 65; // Retain steep braces without extending below the lower screen supports
upper_screen_support_angle = 60; // Raise only the upper brace root, retaining the full pad and socket depth

/* [Single KiCad carrier: F.Cu faces the lid] */
carrier_size = [94.5, 117];
carrier_front_y = 12;
carrier_standoff_height = 5;
carrier_boss_diameter = 7; // Inside the PCB's 3.5 mm-radius screw keepout
carrier_screw_length = 6;
carrier_socket_depth = 5.2;
pcb_thickness = 1.6;
module_clearance = 1;
assembly_clearance = 1;
solder_tail_depth = 3.5;
joystick_window_diameter = 30;
joystick_cap_base_diameter = 26.4; // User's approximate measurement of the cap's bottom, not its swept diameter
joystick_motion_diameter = 29; // Design acceptance limit inside the 30 mm bore, not a measured motion sweep
joystick_body_height = 12.9; // YTL J13 nominal 12.4 plus the drawing's 0.5 mm unspecified tolerance
joystick_total_height = 30; // Height acceptance envelope; cap-bottom diameter alone does not measure height
keycap_size = 18; // PCB acceptance limit, not an identified keycap
keycap_lateral_clearance = 1;
keycap_vertical_clearance = 1;
key_travel = 4.3; // Rated 4 mm travel plus 0.3 mm design allowance
key_window_size = keycap_size + 2 * keycap_lateral_clearance;
key_switch_top_height = 15.45; // CPG151101D13: 11+0.15 body + 4+0.30 released stem
keycap_height = 12; // Common OEM-style cap acceptance limit; exact cap not selected
keycap_stem_engagement = 2; // Minimum overlap, conservative for the released overall height
keycap_top_height = key_switch_top_height + keycap_height - keycap_stem_engagement;
socket_height = 8.7; // LAIL 8.5 +/-0.2, maximum
// Maximum installed heights above the carrier, including sockets and module PCBs.
esp32_stack_height = 20;
// User confirmed their fitted cable touches the left wall. Reserve the whole band, not a short head.
usb_occupied_x = [2.4, 27.75];
usb_occupied_y = [88, 122];
usb_occupied_z = [20.6, 32.6];
usb_wall_contact_confirmed = true;
// User presets measured from the socket top; module PCBs/terminals are included, not added twice.
amplifier_above_socket = 15;
microphone_above_socket = 7.5;
amplifier_stack_height = socket_height + amplifier_above_socket;
amplifier_wire_clearance = 10;
rtc_stack_height = 40;
microphone_stack_height = socket_height + microphone_above_socket;

/* [Landscape TFT: dimensions rotated together] */
tft_size = [108.04, 61.74];
tft_view = [83.52, 55.68];
tft_hole_spacing = [101.9, 54.9];
tft_view_side_margins = [10, 14.52]; // Left and right PCB edges to the active display
tft_window_clearance = 2; // Total enlargement: 1 mm per side
tft_window_radius = 6;
screen_pcb_thickness = 2;
screen_stack_above_pcb = 3;
screen_front_clearance = 1; // Screen-to-lid assembly clearance
screen_back_height = 4;
screen_back_edge_margin = 8;
screen_shift_along = -11.5;
screen_port_clearance = 8;
screen_header_size = [8, 42, 12]; // Provisional upper-right connector/cable reservation

/* [Rear-wall switch] */
switch_hole_diameter = 21;
switch_center = [48, 79]; // Slightly left of case centre, not crowded against the left speaker
switch_screen_clearance = 2;
// Taobao SKU drawing unavailable: retain the existing bore, reserve larger internals.
switch_body_depth = 30;
switch_body_diameter = 24;
switch_wire_depth = 10;
switch_outer_diameter = 28; // Unverified SKU: flange plus full rocker motion acceptance envelope
switch_outer_height = 12;

/* [XHXDZ 2828BOX-4ohm3W-2P1.25 / C49247008] */
speaker_size = [27.5, 27.5, 13];
speaker_tolerance = 0.2;
speaker_hole_spacing = [23, 23];
speaker_grille_diameter = 23.5;
speaker_grille_hole_diameter = 3.8;
speaker_grille_pitch = 6;
speaker_grille_row_pitch = 4.5;
speaker_grille_row_counts = [2, 3, 4, 3, 2];
speaker_center = [104.8, 50.3]; // World Y, Z; raised above USB and shifted forward for screwdriver access
speaker_wall_x = 0;
speaker_tool_tip_diameter = 4;
speaker_tool_space_diameter = 8;
speaker_tool_space_length = 40;
// Archived side view: 13 mm spans ~118 px; front offset/ear thickness span ~28/29 px.
speaker_ear_thickness = 3.2; // Rounded image estimate, not a dimensioned specification
speaker_ear_back_distance = 6.6;
speaker_front_clearance = 0.2;
speaker_front_offset = speaker_size[2] - speaker_ear_thickness - speaker_ear_back_distance;
speaker_post_depth = speaker_front_offset + speaker_front_clearance;
speaker_post_diameter = 4.5; // Metal round nut, not a printed pillar
speaker_sleeve_diameter = insert_diameter + 2 * minimum_print_wall;
speaker_frame_clearance = 0.1;
speaker_sleeve_relief_angle = 50;
speaker_nut_length = 5;
speaker_nut_embed_depth = insert_depth - speaker_post_depth;
speaker_screw_length = 6;
speaker_screw_head_diameter = 5;
speaker_screw_head_depth = 2.5;
microphone_hole_diameter = 6;
microphone_diameter = 14;
microphone_cover_inner_diameter = 16;
microphone_cover_top = 31;

/* [Rear panel Type-C sockets] */
typec_hole_diameter = 12;
typec_hole_spacing = 20;
typec_center_x = 48; // Lower triangle vertices X=38/58; 12 mm left of case centre
typec_center_z = 50.5;
typec_thread_diameter = 10.5;
typec_thread_length = 14.5;
typec_head_diameter = 13.8;
typec_head_thickness = 2;
typec_nut_outer_size = 14;
typec_nut_thickness = 3;
typec_inner_depth = 30; // Provisional body/tail acceptance limit measured from the inner rear wall
typec_inner_diameter = 12; // Not a measured SKU; thread length alone does not bound the complete fitting

/* [Optional cable opening: absent in sketch] */
enable_cable_opening = false;
cable_opening = [16, 10];
cable_opening_z = 10;

/* [Hidden] */
eps = 0.02;
screen_top_y = deck_depth + cos(slope_angle) * slope_length;
rear_ledge = case_depth - screen_top_y;
rear_height = slope_base_height + sin(slope_angle) * slope_length;
deck_transition_y = deck_depth;
flat_deck_end = deck_depth - deck_bend_radius;
// Conservative envelope if the quoted hex size is across flats rather than corners.
typec_nut_envelope = typec_nut_outer_size / cos(30);
screen_board_gap = screen_stack_above_pcb + screen_front_clearance;
screen_mount_gap = screen_pcb_thickness + screen_board_gap;
screen_mount_normal = -panel_thickness - screen_mount_gap;
screen_window_offset = [(tft_view_side_margins[0] - tft_view_side_margins[1]) / 2, 0];
tft_center = [case_width / 2, 46 + screen_shift_along];
tft_rear_extent = deck_depth + cos(slope_angle) * (tft_center[1] + tft_size[1] / 2) -
                  sin(slope_angle) * screen_mount_normal;
carrier_left_x = (case_width - carrier_size[0]) / 2;
carrier_mount_z = floor_thickness + carrier_standoff_height;
carrier_top_z = carrier_mount_z + pcb_thickness;
// Saved PCB coordinates: X=5..99.5, Y=3..120. KiCad +Y points toward the user.
function pcb_point(p) = [carrier_left_x + p[0] - 5, carrier_front_y + 120 - p[1]];
carrier_holes = [for (p = [[10, 8], [94.5, 8], [10, 115], [94.5, 115]]) pcb_point(p)];
key_centers = [for (p = [[70.85, 89.5], [89.9, 89.5],
                         [62.75, 108.55], [81.8, 108.55]]) pcb_point(p)];
keyboard_window_min = [for (axis = [0, 1]) min([for (p = key_centers) p[axis]]) - key_window_size / 2];
keyboard_window_max = [for (axis = [0, 1]) max([for (p = key_centers) p[axis]]) + key_window_size / 2];
keyboard_window_size = keyboard_window_max - keyboard_window_min;
keyboard_row_bounds = [
    for (indices = [[0, 1], [2, 3]])
        let(points = [for (i = indices) key_centers[i]],
            left = min([for (p = points) p[0]]),
            right = max([for (p = points) p[0]]))
            [left - key_window_size / 2, points[0][1] - key_window_size / 2,
             right - left + key_window_size, key_window_size]
];
joystick_center = pcb_point([16.8, 95.5]);
microphone_center = pcb_point([44.455, 90.25]);
screen_mounts = [[-1, -1], [-1, 1], [1, -1]];
speaker_grille_centers = [
    for (row = [0 : 4], column = [0 : speaker_grille_row_counts[row] - 1])
        [speaker_grille_pitch * (column - (speaker_grille_row_counts[row] - 1) / 2),
         speaker_grille_row_pitch * (row - 2)]
];
shell_hole_x = [9, case_width - 9];
shell_hole_y = [11, case_depth - 11];
shell_head_diameter = lid_counterbore_diameter;
profile_vertices = [
    [0, 0], [case_depth, 0], [case_depth, rear_height],
    [screen_top_y, rear_height], [deck_depth, slope_base_height],
    [deck_depth, deck_height], [0, deck_height]
];
profile_radii = [
    bottom_radius, bottom_radius, rim_radius,
    bend_radius, bend_radius, deck_bend_radius, rim_radius
];
profile_count = len(profile_vertices);

function unit(v) = v / norm(v);
function cross2(a, b) = a[0] * b[1] - a[1] * b[0];
function column_bounds(x, y) = [
    x < case_width / 2 ? 0 : x - shell_boss_diameter / 2,
    y < deck_depth ? 0 : y - shell_corner_depth / 2,
    x < case_width / 2 ? x + shell_boss_diameter / 2 : case_width,
    y < deck_depth ? y + shell_corner_depth / 2 : case_depth
];
function rear_brace_rise(x, y) =
    1.5 * tan(support_angle) * (column_bounds(x, y)[2] - column_bounds(x, y)[0] - wall);
// Smoothstep's maximum derivative is 1.5; compensate so even the shallowest segment meets the angle.
function brace_ease(t) = t * t * (3 - 2 * t);
function column_root_z(x, y) =
    y > deck_depth ?
        rear_height - panel_thickness - rear_mount_depth - rear_brace_rise(x, y) :
        floor_thickness;
function reverse_list(a) = [for (i = [len(a) - 1 : -1 : 0]) a[i]];
function corner_angle(i) =
    let(p = profile_vertices[i],
        u = unit(profile_vertices[(i + profile_count - 1) % profile_count] - p),
        v = unit(profile_vertices[(i + 1) % profile_count] - p))
    acos(max(-1, min(1, u * v)));
function tangent_length(i) = profile_radii[i] / tan(corner_angle(i) / 2);

function corner_points(i) =
    let(p = profile_vertices[i],
        u = unit(profile_vertices[(i + profile_count - 1) % profile_count] - p),
        v = unit(profile_vertices[(i + 1) % profile_count] - p),
        angle = corner_angle(i),
        radius = profile_radii[i],
        center = p + unit(u + v) * radius / sin(angle / 2),
        start = p + u * tangent_length(i),
        a0 = atan2(start[1] - center[1], start[0] - center[0]),
        sweep = sign(cross2(-u, v)) * (180 - angle),
        steps = max(4, ceil(abs(sweep) * $fn / 360)))
    // Remove trig drift at tangent endpoints before boolean intersections.
    [for (j = [0 : steps])
        let(q = center + radius * [cos(a0 + sweep * j / steps), sin(a0 + sweep * j / steps)])
            [round(q[0] * 1000000) / 1000000,
             (i == 5 && j == steps) || (i == 6 && j == 0) ? deck_height :
             (i == 3 && j == 0) || (i == 2 && j == steps) ? rear_height :
             round(q[1] * 1000000) / 1000000]];

function top_path(trim = 0) = concat(
    [[rim_radius + trim, deck_height]],
    [for (i = [profile_count - 2 : -1 : 3]) each reverse_list(corner_points(i))],
    [[case_depth - rim_radius - trim, rear_height]]
);

function inward_normal(v) = let(d = unit(v)) [d[1], -d[0]];
// Offset the sampled path with miters so the two halves share identical facets.
function inner_path(path, depth) = [
    for (i = [0 : len(path) - 1])
        let(n1 = inward_normal(path[max(1, i)] - path[max(0, i - 1)]),
            n2 = inward_normal(path[min(len(path) - 1, i + 1)] -
                               path[min(len(path) - 2, i)]),
            p = path[i] + depth * (n1 + n2) / (1 + n1 * n2))
        [round(p[0] * 1000000) / 1000000, round(p[1] * 1000000) / 1000000]
];

// Two front screws and two top/rear screws, all vertical.
assert(part == "assembly" || part == "exploded" || part == "bottom" ||
       part == "lid" || part == "lid-print" || part == "bottom-y-up" ||
       part == "lid-y-up" || part == "none", "Unknown part.");
assert(slope_angle > 30 && slope_angle <= 85, "Screen slope must be between 30 and 85 degrees.");
assert(min(bottom_radius, rim_radius) > wall && bend_radius > panel_thickness,
       "Corner radii must exceed their wall/panel thickness.");
assert(deck_bend_radius > panel_thickness &&
       max(keyboard_window_max[1], joystick_center[1] + joystick_window_diameter / 2) <
       deck_depth - minimum_print_wall,
       "Control apertures must stop before the straight vertical riser.");
assert(floor_thickness >= wall, "Floor thickness must be at least the wall thickness.");
assert(joint_clearance > 0 && joint_clearance < wall, "Invalid lid clearance.");
assert(switch_center[0] - switch_outer_diameter / 2 > bottom_radius &&
       switch_center[0] + switch_outer_diameter / 2 < case_width - bottom_radius &&
       switch_center[1] - switch_outer_diameter / 2 > bottom_radius &&
       switch_center[1] + switch_outer_diameter / 2 < rear_height - rim_radius,
       "Rear switch flange must stay on the flat rear wall.");
assert(screen_port_clearance >= 8, "Screen-to-port edge clearance must be at least 8 mm.");
assert(min(tft_view_side_margins) >= 0 &&
       abs(tft_view_side_margins[0] + tft_view[0] + tft_view_side_margins[1] -
           tft_size[0]) < 0.001, "Display width and side margins must match the PCB.");
assert(tft_window_clearance >= 2, "Screen aperture needs at least 1 mm per side.");
assert(tft_window_radius > 0 &&
       2 * tft_window_radius <= min(tft_view) + tft_window_clearance,
       "Screen window corner radius does not fit its aperture.");
assert(tft_center[1] - tft_size[1] / 2 > tangent_length(4) + lid_min_edge,
       "Lowered screen must clear the lower bend.");
assert(tft_center[1] + tft_size[1] / 2 + module_clearance <
       slope_length - tangent_length(3), "Raised screen overlaps the top bend.");
assert(switch_body_depth > 0 && switch_body_diameter > 0,
       "Switch envelope dimensions must be positive.");
assert(switch_outer_diameter > switch_hole_diameter && switch_outer_height > 0,
       "The switch flange must overlap its bore.");
assert(switch_center[1] - switch_body_diameter / 2 >
       carrier_top_z + rtc_stack_height + assembly_clearance,
       "Rear switch terminals must remain above the RTC.");
assert(joystick_center[0] < microphone_center[0] &&
       microphone_center[0] < min([for (p = key_centers) p[0]]),
       "Microphone must be between the joystick and keys.");
assert(speaker_center[0] - speaker_hole_spacing[0] / 2 - speaker_sleeve_diameter / 2 >
       bottom_radius &&
       speaker_center[0] + speaker_hole_spacing[0] / 2 + speaker_sleeve_diameter / 2 <
       case_depth - bottom_radius &&
       speaker_center[1] - speaker_size[1] / 2 - speaker_tolerance > bottom_radius,
       "Left speaker must clear the bottom and rear corner fillets.");
assert(speaker_wall_x == 0, "This revision mounts the speaker on the left wall.");
assert(usb_wall_contact_confirmed && usb_occupied_x[0] == wall &&
       usb_occupied_x[1] == 27.75, "Preserve the user's confirmed wall-contact cable corridor.");
assert(speaker_center[1] - speaker_size[1]/2 - speaker_tolerance >=
       usb_occupied_z[1] + assembly_clearance, "Speaker body must clear the occupied USB band, not just the ESP32 PCB.");
assert(speaker_tool_tip_diameter + 2*assembly_clearance <= 6,
       "Driver tip must fit the speaker's 6 mm rear corner well.");
assert(speaker_post_depth > 0 && speaker_nut_embed_depth > 0 &&
       panel_thickness - speaker_nut_embed_depth >= minimum_print_wall - eps,
       "Speaker insert pockets must retain the minimum printed blind floor.");
assert(speaker_post_diameter > 3 && speaker_post_diameter < insert_diameter &&
       speaker_screw_length - speaker_ear_thickness >= 2 &&
       speaker_screw_length - speaker_ear_thickness <= speaker_nut_length - 0.3,
       "Speaker inserts must fit the sleeves; screws must only engage part of the nut.");
assert(speaker_sleeve_diameter >= insert_diameter + 2 * minimum_print_wall &&
       speaker_sleeve_relief_angle > 0 && speaker_sleeve_relief_angle < 90,
       "Speaker sleeves need thick walls and an inward-facing relief.");
assert(sqrt(pow(norm(speaker_hole_spacing / 2), 2) + pow(speaker_sleeve_diameter / 2, 2) -
            2 * norm(speaker_hole_spacing / 2) * speaker_sleeve_diameter / 2 *
            cos(speaker_sleeve_relief_angle)) -
       (speaker_size[0] / 2 + speaker_tolerance) >= speaker_frame_clearance,
       "The sleeve relief must clear the complete round speaker frame.");
for (dx = [-speaker_hole_spacing[0] / 2, speaker_hole_spacing[0] / 2],
     dy = [-speaker_hole_spacing[1] / 2, speaker_hole_spacing[1] / 2],
     p = speaker_grille_centers)
    assert(norm([dx,dy] - p) >
           (insert_diameter + speaker_grille_hole_diameter) / 2,
           "Speaker posts must not obstruct any grille hole.");
assert(speaker_grille_pitch - speaker_grille_hole_diameter >= 1.2 - eps &&
       len(speaker_grille_centers) > 0, "The grille needs at least 1.2 mm webs.");
for (i = [0 : len(speaker_grille_centers) - 2],
     j = [i + 1 : len(speaker_grille_centers) - 1])
    assert(norm(speaker_grille_centers[i] - speaker_grille_centers[j]) -
           speaker_grille_hole_diameter >= 1.2 - eps,
           "Staggered grille rows need at least 1.2 mm webs.");
for (p = speaker_grille_centers)
    assert(norm(p) + speaker_grille_hole_diameter / 2 <= speaker_grille_diameter / 2,
           "Every grille hole must stay inside the circular area.");
assert(typec_hole_diameter > typec_thread_diameter &&
       typec_head_diameter > typec_hole_diameter,
       "Type-C thread needs clearance and the flange must overlap the panel hole.");
assert(typec_thread_length > wall + typec_nut_thickness &&
       typec_hole_spacing > max(typec_head_diameter, typec_nut_envelope),
       "Type-C sockets need room for both nuts and thread engagement.");
assert(typec_inner_depth >= typec_thread_length-wall &&
       typec_inner_diameter >= typec_thread_diameter,
       "Full inner-body reservation must include the threaded projection.");
assert(typec_center_z - typec_nut_envelope / 2 > bottom_radius &&
       typec_center_z + typec_nut_envelope / 2 < rear_height - rim_radius,
       "Type-C fittings must remain on the flat rear wall.");
assert(typec_center_x < case_width / 2 &&
       typec_center_x - typec_hole_spacing / 2 - typec_nut_envelope / 2 > bottom_radius &&
       typec_center_x + typec_hole_spacing / 2 + typec_nut_envelope / 2 < case_width - bottom_radius,
       "Both Type-C fittings must clear the rear corners and be shifted left.");
assert(switch_center[0] == typec_center_x &&
       switch_center[1] - switch_outer_diameter/2 >
       typec_center_z + typec_nut_envelope/2 + assembly_clearance,
       "Rear triangle needs an upper switch and clearance to both complete Type-C fittings.");
assert(lid_corner_radius > joint_clearance && support_lip_width > joint_clearance,
       "The rounded opening and support lip must retain lid clearance and bearing overlap.");
assert(support_lip_thickness >= 1.5, "Support lip needs sufficient printing thickness.");
assert(panel_thickness >= minimum_print_wall &&
       joystick_center[0] - joystick_window_diameter / 2 - wall - joint_clearance >=
           minimum_print_wall &&
       case_width - wall - joint_clearance -
           (keyboard_row_bounds[0][0] + keyboard_row_bounds[0][2] + minimum_print_wall) >=
           minimum_print_wall &&
       microphone_center[0] - microphone_cover_inner_diameter / 2 -
           joystick_center[0] - joystick_window_diameter / 2 >= minimum_print_wall,
       "Control openings must retain the minimum printed wall and side webs.");
assert(lid_counterbore_diameter > lid_screw_diameter &&
       lid_counterbore_depth > 0 && panel_thickness - lid_counterbore_depth >= 1.6,
       "Counterbores must leave at least 1.6 mm of lid under the screw head.");
assert(shell_corner_depth >= insert_diameter + 2,
       "Corner screw seats need at least 1 mm around the glue sockets.");
assert(support_angle >= 45 && support_angle < 90, "Brace angle must be in [45, 90) degrees.");
assert(upper_screen_support_angle >= 45 && upper_screen_support_angle <= support_angle,
       "Upper screen brace must remain at least 45 degrees with the full pad retained.");
assert(rear_mount_depth >= max(insert_depth, screw_tip_depth) + 1.2,
       "Rear seats must retain at least 1.2 mm below the nut and screw-tip pockets.");
assert(carrier_mount_z - max(insert_depth, carrier_socket_depth) >= 1.5,
       "Carrier blind sockets must leave a solid floor.");
assert(carrier_screw_length > pcb_thickness &&
       carrier_screw_length - pcb_thickness + 0.2 <= carrier_socket_depth,
       "Use M2.5x6 screws for the carrier.");
assert(carrier_standoff_height >= solder_tail_depth + assembly_clearance,
       "Carrier solder tails need floor clearance.");
assert(carrier_boss_diameter >= insert_diameter + 2 && carrier_boss_diameter <= 7,
       "Carrier posts must stay within the PCB screw keepouts.");
assert(joystick_window_diameter > 0 &&
       joystick_center[0] - joystick_window_diameter / 2 > wall + joint_clearance &&
       joystick_center[1] - joystick_window_diameter / 2 > rim_radius + joint_clearance,
       "Joystick circular opening overlaps the lid boundary.");
assert(joystick_cap_base_diameter < joystick_motion_diameter &&
       joystick_motion_diameter + 1 <= joystick_window_diameter,
       "Cap base and accepted motion envelope must fit inside the specified joystick bore.");
assert(keycap_lateral_clearance >= 1 && keycap_vertical_clearance >= 1 && key_travel >= 4,
       "Keycaps need full-travel clearance, >=1 mm laterally and >=1 mm vertically.");
assert(keyboard_row_bounds[0][1] < keyboard_row_bounds[1][1] + keyboard_row_bounds[1][3],
       "The two staggered row openings must overlap; do not leave a separator or point contact.");
assert(microphone_cover_inner_diameter >= microphone_diameter + 2 * module_clearance &&
       microphone_hole_diameter < microphone_cover_inner_diameter &&
       microphone_cover_top > deck_height &&
       carrier_top_z + microphone_stack_height + module_clearance <=
           microphone_cover_top - panel_thickness,
       "The microphone needs a raised cover with PCB and stack clearance.");
assert(screen_front_clearance >= module_clearance, "Insufficient screen-to-lid clearance.");
assert(screen_pcb_thickness > 0 && screen_stack_above_pcb > 0,
       "Screen PCB and display thicknesses must be positive.");
assert(screen_tab_depth > screw_tip_depth &&
       deck_height - panel_thickness - floor_thickness > screw_tip_depth,
       "Mounting tabs need blind clearance for the screw tips.");
assert(screen_gusset_thickness >= 2, "Screen gussets must be at least 2 mm thick.");
assert(screw_length - pcb_thickness <= screw_tip_depth &&
       screw_length - screen_pcb_thickness <= screw_tip_depth &&
       screw_length - (panel_thickness - lid_counterbore_depth) <= screw_tip_depth,
       "Screws are too long for the blind holes.");
assert(insert_depth < screw_tip_depth && insert_diameter < module_boss_diameter - 2,
       "Insufficient insert depth or boss wall.");
assert(tft_size[0] + 2 * (wall + joint_clearance) < case_width,
       "Landscape screen does not fit between the side walls.");
assert(tft_size[1] + tangent_length(3) + tangent_length(4) +
       2 * module_clearance < slope_length,
       "TFT overlaps the curved transitions.");
assert(rear_ledge > tangent_length(2) + tangent_length(3) + 4,
       "Rear ledge is too short.");
assert(tft_rear_extent + module_clearance < case_depth - wall &&
       carrier_front_y + carrier_size[1] + 4 <= case_depth - wall,
       "Screen and carrier need rear-wall clearance.");
for (i = [0 : profile_count - 1])
    assert(tangent_length(i) + tangent_length((i + 1) % profile_count) <
           norm(profile_vertices[(i + 1) % profile_count] - profile_vertices[i]),
           "Fillets overlap: reduce radii or enlarge the profile.");
for (p = key_centers)
    assert(p[0] - key_window_size / 2 > wall + joint_clearance &&
           p[0] + key_window_size / 2 < case_width - wall - joint_clearance &&
           p[1] - key_window_size / 2 > rim_radius + joint_clearance,
           "Key opening overlaps the lid boundary.");

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
    // The spherical volume alone defines the lower fillets; a second sampled
    // bottom arc would intersect the same surface at almost coincident points.
    polygon(concat([[0, -bottom_radius], [case_depth, -bottom_radius]],
                   [for (i = [2 : profile_count - 1]) each corner_points(i)]));
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
    }
}

module on_slope(x, along, normal = 0) {
    translate([x, deck_depth + cos(slope_angle) * along,
               slope_base_height + sin(slope_angle) * along])
        rotate([slope_angle, 0, 0])
            translate([0, 0, normal]) children();
}

// Mirror the complete speaker assembly to the left; local +X still points toward the rear.
module on_speaker(u, v, normal = 0) {
    multmatrix([[0, 0, -1, speaker_wall_x], [1, 0, 0, u],
                [0, 1, 0, v], [0, 0, 0, 1]])
        translate([0, 0, normal]) children();
}

module on_switch(normal = 0) {
    translate([switch_center[0], case_depth, switch_center[1]])
        rotate([90, 0, 0])
            translate([0, 0, normal]) children();
}

module carrier_fasteners() {
    for (p = carrier_holes)
        translate([p[0], p[1], carrier_mount_z]) children();
}

module screen_fasteners() {
    for (p = screen_mounts)
        on_slope(tft_center[0] + p[0] * tft_hole_spacing[0] / 2,
                 tft_center[1] + p[1] * tft_hole_spacing[1] / 2,
                 screen_mount_normal) children();
}

// The opening is at Z=0; screw and insert extend into negative Z.
module insert_socket(tip_depth = screw_tip_depth) {
    translate([0, 0, -insert_depth])
        cylinder(d = insert_diameter, h = insert_depth + eps);
    translate([0, 0, -tip_depth])
        cylinder(d = screw_clearance, h = tip_depth + eps);
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
            root = column_root_z(x, y);
            width = bounds[2] - bounds[0];
            if (y > deck_depth) {
                rise = rear_brace_rise(x, y);
                steps = max(8, $fn);
                // Absolute Z avoids separately quantizing (top-root) and root into a sliver.
                profile = concat([[0, root], [wall, root]],
                    [for (i = [1 : steps])
                        [wall + (width - wall) * brace_ease(i / steps), root + rise * i / steps]],
                    [[width, top], [0, top]]);
                translate([x < case_width / 2 ? bounds[0] : bounds[2], bounds[3], 0])
                    rotate([90, 0, 0])
                        linear_extrude(height = bounds[3] - bounds[1])
                            polygon([for (p = profile)
                                [x < case_width / 2 ? p[0] : -p[0], p[1]]]);
            } else
                // Match the fractional roof Z directly, without quantizing root + height twice.
                translate([bounds[0], bounds[1], 0])
                    extrude_along_y(bounds[3] - bounds[1])
                        polygon([[0, floor_thickness - eps], [width, floor_thickness - eps],
                                 [width, top], [0, top]]);
        }
}

module screen_tab_pad(side, x, height) {
    hull() {
        translate([x, 0, 0]) cylinder(d = module_boss_diameter, h = height);
        // A rectangular root crosses the wall instead of meeting its inner face tangentially.
        translate([side < 0 ? 0 : case_width - wall - 0.5, -panel_thickness / 2, 0])
            cube([wall + 0.5, panel_thickness, height]);
    }
}

module screen_tabs() {
    for (p = screen_mounts) {
            side = p[0];
            v = p[1] * tft_hole_spacing[1] / 2;
            x = tft_center[0] + side * tft_hole_spacing[0] / 2;
            along = tft_center[1] + v;
            base_normal = screen_mount_normal - screen_tab_depth;
            reach = (side < 0 ? x : case_width - x) + module_boss_diameter / 2 - wall;
            base_y = deck_depth + cos(slope_angle) * along - sin(slope_angle) * base_normal;
            base_z = slope_base_height + sin(slope_angle) * along + cos(slope_angle) * base_normal;
            front_y = deck_depth + cos(slope_angle) * along -
                      sin(slope_angle) * screen_mount_normal -
                      module_boss_diameter / 2 * cos(slope_angle);
            back_y = base_y + module_boss_diameter / 2 * cos(slope_angle);
            brace_angle = p[1] > 0 ? upper_screen_support_angle : support_angle;
            root_z = base_z - module_boss_diameter / 2 * sin(slope_angle) -
                     reach * tan(brace_angle) - screen_gusset_thickness;
            assert(root_z >= floor_thickness, "Screen brace reaches below the usable floor.");
            // Cover the entire WORLD-Y projection, including directly below the hole.
            // The entire root is below the lowest pad surface by the specified brace slope.
            hull() {
                on_slope(0, along, base_normal)
                    screen_tab_pad(side, x, screen_tab_depth);
                translate([side < 0 ? 0 : case_width - wall,
                           front_y, root_z])
                    cube([wall, back_y - front_y, screen_gusset_thickness]);
            }
        }
}

module rear_typec_positions() {
    for (dx = [-typec_hole_spacing / 2, typec_hole_spacing / 2])
        translate([typec_center_x + dx, case_depth, typec_center_z])
            rotate([90, 0, 0]) children();
}

module typec_envelopes(inset = 0) {
    rear_typec_positions() {
        translate([0,0,wall+inset])
            cylinder(d=typec_inner_diameter-2*inset,h=typec_inner_depth-2*inset);
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
    on_switch(wall + inset)
        cylinder(d = switch_body_diameter - 2 * inset,
                 h = switch_body_depth + switch_wire_depth - 2 * inset);
}

module switch_outer_envelope(inset = 0) {
    on_switch(-switch_outer_height + inset)
        cylinder(d = switch_outer_diameter - 2 * inset, h = switch_outer_height - 2 * inset);
}

module speaker_fasteners() {
    for (dx = [-speaker_hole_spacing[0] / 2, speaker_hole_spacing[0] / 2])
        for (dy = [-speaker_hole_spacing[1] / 2, speaker_hole_spacing[1] / 2])
            on_speaker(speaker_center[0] + dx, speaker_center[1] + dy,
                     -panel_thickness - speaker_post_depth) children();
}

module speaker_posts(inset = 0) {
    speaker_fasteners()
        translate([0,0,inset])
            difference() {
                cylinder(d = speaker_post_diameter - 2 * inset,
                         h = speaker_nut_length - 2 * inset);
                translate([0,0,-eps])
                    cylinder(d = 2.5 + 2 * inset,
                             h = speaker_nut_length + 2 * eps);
            }
}

module speaker_nut_pockets() {
    speaker_fasteners()
        rotate([180,0,180 / $fn])
            insert_socket(insert_depth);
}

module speaker_sleeves() {
    for (dx = [-speaker_hole_spacing[0] / 2, speaker_hole_spacing[0] / 2],
         dy = [-speaker_hole_spacing[1] / 2, speaker_hole_spacing[1] / 2])
        on_speaker(speaker_center[0] + dx, speaker_center[1] + dy,
                 -panel_thickness - speaker_post_depth)
            difference() {
                rotate([0,0,3])
                    cylinder(d = speaker_sleeve_diameter, h = speaker_post_depth + eps);
                // A sector relief avoids thin tapered tips left by a circular frame cut.
                rotate([0,0,atan2(-dy,-dx)])
                    translate([0,0,-eps])
                        linear_extrude(height = speaker_post_depth + 3 * eps)
                            polygon([[0,0],
                                     [10*cos(speaker_sleeve_relief_angle),
                                      10*sin(speaker_sleeve_relief_angle)],
                                     [10*cos(speaker_sleeve_relief_angle),
                                      -10*sin(speaker_sleeve_relief_angle)]]);
            }
}

module speaker_mount_envelope(clearance = 0) {
    speaker_fasteners()
        translate([0, 0, -clearance])
            cylinder(d = speaker_sleeve_diameter + 2 * clearance,
                     h = speaker_post_depth + 2 * clearance);
}

module speaker_tool_envelope(inset = 0) {
    speaker_fasteners() {
        // Tip passes through the 6 mm corner well; wider working space starts behind the body.
        translate([0,0,-10.2 + inset])
            cylinder(d = speaker_tool_tip_diameter - 2*inset, h = 4.5 - 2*inset);
        translate([0,0,-10.2 - speaker_tool_space_length + inset])
            cylinder(d = speaker_tool_space_diameter - 2*inset,
                     h = speaker_tool_space_length - 2*inset);
    }
}

module speaker_body_envelope(inset = 0) {
    rear = -panel_thickness - speaker_front_clearance - speaker_size[2] - speaker_tolerance;
    ear_front = -panel_thickness - speaker_post_depth;
    ear_back = ear_front - speaker_ear_thickness;
    size = speaker_size[0] + 2 * (speaker_tolerance - inset);
    difference() {
        union() {
            on_speaker(speaker_center[0] - size / 2,
                     speaker_center[1] - size / 2, rear + inset)
                cube([size, size, ear_front - rear - 2 * inset]);
            on_speaker(speaker_center[0], speaker_center[1], ear_front + inset)
                cylinder(d = size, h = speaker_front_offset - 2 * inset);
        }
        // Rear corner wells expose the front ears; only the ear plate has a 3 mm screw bore.
        speaker_fasteners() {
            translate([0,0,rear - ear_front - 1])
                cylinder(d = 6 + 2 * inset, h = ear_back - rear + 1 + inset);
            translate([0,0,-speaker_ear_thickness - eps])
                cylinder(d = 3 + 2 * inset, h = speaker_ear_thickness + 2 * eps);
        }
    }
}

module speaker_envelope(inset = 0) {
    speaker_body_envelope(inset);
    speaker_fasteners()
        translate([0, 0, -speaker_ear_thickness - speaker_screw_head_depth + inset])
            cylinder(d = speaker_screw_head_diameter - 2 * inset,
                     h = speaker_screw_head_depth - 2 * inset);
    // Local +X now points toward the rear; reserve the first 10 mm of lead.
    on_speaker(speaker_center[0] + speaker_size[0] / 2,
             speaker_center[1] - 3 + inset,
             -panel_thickness - speaker_front_clearance - speaker_size[2] - speaker_tolerance + inset)
        cube([10, 6 - 2 * inset, 6 - 2 * inset]);
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
                carrier_fasteners()
                    translate([0, 0, -carrier_standoff_height - eps])
                        cylinder(d = carrier_boss_diameter, h = carrier_standoff_height + eps);
                screen_tabs();
                shell_mount_columns();
                // Thicken only this patch to 3 mm, retaining the original blind nut floors.
                on_speaker(speaker_center[0], speaker_center[1], -panel_thickness)
                    linear_extrude(height = panel_thickness - wall + eps)
                        rounded_rectangle_centered(
                            speaker_hole_spacing + [speaker_sleeve_diameter + 2,
                                                    speaker_sleeve_diameter + 2], 3);
                speaker_sleeves();
            }
        }
        carrier_fasteners() insert_socket(carrier_socket_depth);
        screen_fasteners() insert_socket();
        shell_fasteners() insert_socket();
        carrier_installation_clearance();
        speaker_grille();
        speaker_nut_pockets();
        on_switch(-eps)
            cylinder(d = switch_hole_diameter, h = wall + 2 * eps);
        rear_typec_positions()
            translate([0, 0, -eps])
                cylinder(d = typec_hole_diameter, h = wall + 2 * eps);
        if (enable_cable_opening)
            translate([(case_width - cable_opening[0]) / 2,
                       case_depth - wall - eps, cable_opening_z])
                cube([cable_opening[0], wall + 2 * eps, cable_opening[1]]);
    }
}

module keyboard_opening_profile() {
    union()
        for (row = keyboard_row_bounds)
            translate([row[0], row[1]]) square([row[2], row[3]]);
}

module control_vertical_openings() {
    translate([0, 0, deck_height - panel_thickness - eps])
        linear_extrude(height = panel_thickness + deck_bend_radius + 2 * eps)
            keyboard_opening_profile();
    translate([joystick_center[0], joystick_center[1],
               deck_height - panel_thickness - eps])
        cylinder(d = joystick_window_diameter,
                 h = panel_thickness + deck_bend_radius + 2 * eps);
}

module rounded_rectangle_centered(size, radius) {
    translate(-size / 2) rounded_rectangle(size, radius);
}

module speaker_grille() {
        for (p = speaker_grille_centers)
            // Keep cutter cap diagonals away from the panel faces to avoid micron-scale slivers.
            on_speaker(speaker_center[0] + p[0], speaker_center[1] + p[1], -panel_thickness - 1)
                // Mirror circle phases and avoid alignment with the panel's long facet diagonals.
                rotate([0,0,sign(p[0]) * (3 +
                       (abs(p[0]) + abs(p[1])) / speaker_grille_pitch * 180 / $fn / 2)])
                    cylinder(d = speaker_grille_hole_diameter, h = panel_thickness + 2);
}

module lid_openings() {
        control_vertical_openings();
        translate([microphone_center[0], microphone_center[1],
                   deck_height - panel_thickness - eps])
            cylinder(d = microphone_cover_inner_diameter,
                     h = microphone_cover_top - deck_height + eps);
        translate([microphone_center[0], microphone_center[1],
                   microphone_cover_top - panel_thickness - eps])
            cylinder(d = microphone_hole_diameter, h = panel_thickness + 2 * eps);
        on_slope(tft_center[0] + screen_window_offset[0] -
                     (tft_view[0] + tft_window_clearance) / 2,
                 tft_center[1] + screen_window_offset[1] -
                     (tft_view[1] + tft_window_clearance) / 2,
                 -panel_thickness - eps)
            linear_extrude(height = panel_thickness + 2 * eps)
                rounded_rectangle(tft_view + [tft_window_clearance, tft_window_clearance],
                                  tft_window_radius);
        shell_fasteners()
            translate([0, 0, -eps])
                cylinder(d = lid_screw_diameter, h = panel_thickness + wall);
        shell_fasteners()
            translate([0, 0, panel_thickness - lid_counterbore_depth])
                cylinder(d = lid_counterbore_diameter, h = lid_counterbore_depth + eps);
}

module microphone_cover() {
    radius = microphone_cover_inner_diameter / 2 + wall;
    translate([microphone_center[0], microphone_center[1], 0])
        rotate_extrude(convexity = 10)
            // Bury the cylinder base inside the deck, not on its curved underside.
            polygon([[0, deck_height - 0.1],
                     [radius, deck_height - 0.1],
                     [radius, microphone_cover_top], [0, microphone_cover_top]]);
}

module lid() {
    difference() {
        union() {
            lid_blank();
            microphone_cover();
        }
        lid_openings();
    }
}

module carrier_board(inset = 0) {
    difference() {
        translate([carrier_left_x + inset, carrier_front_y + inset, carrier_mount_z + inset])
            linear_extrude(height = pcb_thickness - 2 * inset)
                rounded_rectangle(carrier_size - [2 * inset, 2 * inset], 5 - inset);
        carrier_fasteners() translate([0, 0, -eps])
            cylinder(d = 3, h = pcb_thickness + 2 * eps);
    }
}

module carrier_clearance_plan() {
    translate([carrier_left_x - assembly_clearance, carrier_front_y - assembly_clearance])
        rounded_rectangle(carrier_size + [2 * assembly_clearance, 2 * assembly_clearance],
                          5 + assembly_clearance);
}

module carrier_installation_clearance() {
    difference() {
        union() {
            translate([0, 0, carrier_mount_z - solder_tail_depth - assembly_clearance])
                linear_extrude(height = pcb_thickness + solder_tail_depth + 2 * assembly_clearance)
                    carrier_clearance_plan();
            // Extend only the front-seat relief upward so the leveled PCB can descend.
            // Screen and rear supports retain their full nut-pocket walls.
            intersection() {
                translate([0, 0, carrier_mount_z - solder_tail_depth - assembly_clearance])
                    linear_extrude(height = deck_height) carrier_clearance_plan();
                cube([case_width, shell_hole_y[0] + shell_corner_depth / 2 + assembly_clearance,
                      deck_height + floor_thickness]);
            }
        }
        carrier_fasteners()
            translate([0, 0, -carrier_standoff_height - 1])
                cylinder(d = carrier_boss_diameter + 2 * eps,
                         h = carrier_standoff_height + pcb_thickness + assembly_clearance + 2);
    }
}

module screen_board(inset = 0) {
    assert(inset >= 0 && 2 * inset < min(pcb_thickness, screen_pcb_thickness),
           "Invalid PCB inspection inset.");
    on_slope(tft_center[0] - tft_size[0] / 2 + inset,
             tft_center[1] - tft_size[1] / 2 + inset, screen_mount_normal + inset)
        cube([tft_size[0] - 2 * inset, tft_size[1] - 2 * inset, screen_pcb_thickness - 2 * inset]);
}

module pcb_box(bounds, height, inset = 0, base = carrier_top_z) {
    p = pcb_point([bounds[0], bounds[3]]);
    translate([p[0] + inset, p[1] + inset, base + inset])
        cube([bounds[2] - bounds[0] - 2 * inset, bounds[3] - bounds[1] - 2 * inset,
              height - 2 * inset]);
}

module amplifier_envelope(inset = 0) {
    pcb_box([35, 48, 62, 72], amplifier_stack_height, inset);
    // Full-footprint upper allowance deliberately overbounds the unknown terminal location.
    pcb_box([35, 48, 62, 72], amplifier_wire_clearance, inset,
            carrier_top_z + amplifier_stack_height);
}

module usb_cable_envelope(inset = 0) {
    translate([usb_occupied_x[0]+inset,usb_occupied_y[0]+inset,usb_occupied_z[0]+inset])
        cube([usb_occupied_x[1]-usb_occupied_x[0]-2*inset,
              usb_occupied_y[1]-usb_occupied_y[0]-2*inset,
              usb_occupied_z[1]-usb_occupied_z[0]-2*inset]);
}

module joystick_body_envelope(inset = 0) {
    // YTL J13 p1: 21 x 17 asymmetric body, 13.15 central frame, 8.6-wide side switch.
    // KiCad -90 degrees maps drawing Y to case X and drawing X to case Y.
    // Enclose the two real regions separately, including 0.5/0.3 plan tolerances.
    translate([joystick_center[0]-8.1+inset,joystick_center[1]-7.075+inset,carrier_top_z+inset])
        cube([18-2*inset,16.975-2*inset,joystick_body_height-2*inset]);
    translate([joystick_center[0]-4.6+inset,joystick_center[1]-12.1+inset,carrier_top_z+inset])
        cube([9.2-2*inset,6.025-2*inset,joystick_body_height-2*inset]);
}

module keycap_swept_envelope(inset = 0) {
    for (p = key_centers)
        translate([p[0] - keycap_size / 2 - keycap_lateral_clearance + inset,
                   p[1] - keycap_size / 2 - keycap_lateral_clearance + inset,
                   carrier_top_z + key_switch_top_height - keycap_stem_engagement -
                       key_travel - keycap_vertical_clearance + inset])
            cube([key_window_size - 2 * inset, key_window_size - 2 * inset,
                  keycap_height + key_travel + 2 * keycap_vertical_clearance - 2 * inset]);
}

module carrier_component_envelope(index, inset = 0, inserting = false) {
    assert(index >= 0 && index < 14 && floor(index) == index, "Unknown carrier component.");
    if (index == 0) pcb_box([20, 18.03, 88.5, 45.97], esp32_stack_height, inset);
    if (index == 1) amplifier_envelope(inset);
    if (index == 2) pcb_box([60.5, 3, 88, 15], inserting ? socket_height : rtc_stack_height, inset);
    // J6 display cable connector, including a 10 mm cable bend above the socket.
    if (index == 3) pcb_box([21.65, 8.26, 59.15, 11.74], socket_height + 10, inset);
    // Exact rotated WJ500V bodies plus joining lugs, bounded with 0.5 mm tolerance.
    if (index == 4) pcb_box([5.08, 45.47, 16.08, 56.83], 14.57, inset);
    if (index == 5) pcb_box([5.08, 57.97, 16.08, 69.33], 14.57, inset);
    if (index == 6) pcb_box([84.16, 51.55, 95.16, 67.99], 14.57, inset);
    if (index == 7)
        translate([microphone_center[0], microphone_center[1], carrier_top_z + inset])
            cylinder(d = microphone_diameter - 2 * inset, h = microphone_stack_height - 2 * inset);
    if (index >= 8 && index <= 11)
        let(p = key_centers[index - 8])
            translate([p[0] - keycap_size / 2 + inset, p[1] - keycap_size / 2 + inset,
                       carrier_top_z + inset])
                cube([keycap_size - 2 * inset, keycap_size - 2 * inset,
                      keycap_top_height - 2 * inset]);
    if (index == 12) joystick_body_envelope(inset);
    if (index == 13)
        translate([joystick_center[0], joystick_center[1], carrier_top_z + inset])
            cylinder(d = (inserting ? 4.5 : joystick_motion_diameter) - 2 * inset,
                     h = (inserting ? 19.45 : joystick_total_height) - 2 * inset);
}

module carrier_components(inset = 0) {
    for (index = [0 : 13]) carrier_component_envelope(index, inset);
}

module carrier_solder_envelope(inset = 0) {
    difference() {
        translate([carrier_left_x + inset, carrier_front_y + inset,
                   carrier_mount_z - solder_tail_depth + inset])
            linear_extrude(height = solder_tail_depth - 2 * inset)
                rounded_rectangle(carrier_size - [2 * inset, 2 * inset], 5 - inset);
        carrier_fasteners() translate([0, 0, -solder_tail_depth - 1])
            cylinder(d = 7 + 2 * inset, h = solder_tail_depth + 2);
    }
}

module screen_header_envelope(inset = 0) {
    on_slope(tft_center[0] + tft_size[0] / 2 - screen_header_size[0] + inset,
             tft_center[1] + tft_size[1] / 2 - screen_header_size[1] + inset,
             screen_mount_normal - screen_header_size[2] + inset)
        cube(screen_header_size - [2 * inset, 2 * inset, 2 * inset]);
}

module screen_envelope(inset = 0) {
    screen_board(inset);
    screen_header_envelope(inset);
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

module module_envelopes(inset = 0) {
    carrier_board(inset);
    carrier_components(inset);
    carrier_solder_envelope(inset);
    screen_envelope(inset);
    usb_cable_envelope(inset);
}

module printable_lid() {
    // Integer X shift preserves decimal-grid precision after the rigid print rotation.
    translate([ceil(rear_height), -rim_radius - joint_clearance, -wall - joint_clearance])
        rotate([0, -90, 0]) lid();
}

if (part == "bottom") {
    bottom_shell();
} else if (part == "lid") {
    lid();
} else if (part == "lid-print") {
    printable_lid();
} else if (part == "bottom-y-up") {
    translate([0, 0, case_depth])
        rotate([-90, 0, 0]) bottom_shell();
} else if (part == "lid-y-up") {
    translate([0, 0, case_depth - 2 * (rim_radius + joint_clearance)])
        rotate([-90, 0, 0]) printable_lid();
} else if (part == "assembly" || part == "exploded") {
    color([0.82, 0.82, 0.84]) bottom_shell();
    translate([0, 0, part == "exploded" ? explode_height : 0]) {
        color([0.82, 0.82, 0.84]) lid();
    }
    if (show_modules) {
        %module_envelopes();
        %typec_envelopes();
        %switch_envelope();
        %switch_outer_envelope();
        %speaker_envelope();
    }
}
