import unittest
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest.mock import patch

import check_enclosure


class MeshChecksTest(unittest.TestCase):
    def setUp(self):
        a, b, c, d = (0, 0, 0), (1, 0, 0), (0, 1, 0), (0, 0, 1)
        self.closed = [[a, c, b], [a, b, d], [a, d, c], [b, c, d]]

    def check(self, triangles):
        return check_enclosure.mesh_measurements(triangles, 2, "test mesh")

    def test_closed_outward_shell(self):
        bounds, volume = self.check(self.closed)
        self.assertEqual(bounds, [(0, 1)] * 3)
        self.assertAlmostEqual(volume, 1 / 6)

    def test_distinct_but_collinear_vertices(self):
        with self.assertRaisesRegex(AssertionError, "Zero-area triangle"):
            self.check([[(0, 0, 0), (1, 0, 0), (2, 0, 0)]])

    def test_open_hole(self):
        with self.assertRaisesRegex(AssertionError, "Non-manifold/open edges"):
            self.check(self.closed[:-1])

    def test_reverse_face(self):
        with self.assertRaisesRegex(AssertionError, "Inconsistent face orientation"):
            self.check([self.closed[0][::-1], *self.closed[1:]])

    def test_inverted_shell(self):
        with self.assertRaisesRegex(AssertionError, "Inverted or empty mesh"):
            self.check([triangle[::-1] for triangle in self.closed])

    def test_duplicate_face(self):
        with self.assertRaisesRegex(AssertionError, "Duplicate triangle"):
            self.check([*self.closed, self.closed[0]])

    def test_separate_shells(self):
        translated = [[tuple(v + 3 for v in p) for p in t] for t in self.closed]
        with self.assertRaisesRegex(AssertionError, "Multiple edge-connected shells"):
            self.check(self.closed + translated)

    def test_shells_touching_at_only_one_vertex(self):
        reflected = [[tuple(-v for v in p) for p in t[::-1]] for t in self.closed]
        with self.assertRaisesRegex(AssertionError, "Multiple edge-connected shells"):
            self.check(self.closed + reflected)

    def test_submicron_edges(self):
        tiny = [[tuple(v * 0.0001 for v in p) for p in t] for t in self.closed]
        with patch.object(check_enclosure, "read_triangles", return_value=tiny):
            with self.assertRaisesRegex(AssertionError, "Submicron edge"):
                check_enclosure.mesh_info(Path("tiny.3mf"), 2)

    def test_rounding_collapses_distinct_collinear_face(self):
        nearly_flat = [
            [(0, 0, 0), (1, 0, 0), (2, 0.00001, 0)],
            [(0, 0, 0), (0, 0, 1), (1, 0, 0)],
            [(0, 0, 0), (2, 0.00001, 0), (0, 0, 1)],
            [(1, 0, 0), (0, 0, 1), (2, 0.00001, 0)],
        ]
        # Outward tetrahedron, all edges long, but one altitude is below upload precision.
        nearly_flat = [t[::-1] for t in nearly_flat]
        self.check(nearly_flat)
        with patch.object(check_enclosure, "read_triangles", return_value=nearly_flat):
            with self.assertRaisesRegex(AssertionError, "Zero-area triangle"):
                check_enclosure.mesh_info(Path("nearly-flat.3mf"), 2)

    def test_current_carrier_alignment(self):
        check_enclosure.check_board()

    def test_moved_carrier_hole_is_rejected(self):
        original = check_enclosure.BOARD.read_text(encoding="utf-8")
        changed = original.replace("(at 10 8)", "(at 11 8)", 1)
        self.assertNotEqual(original, changed)
        with patch.object(Path, "read_text", return_value=changed):
            with self.assertRaisesRegex(AssertionError, "PCB H1 moved"):
                check_enclosure.check_board()

    def test_changed_pcb_corner_is_rejected(self):
        original = check_enclosure.BOARD.read_text(encoding="utf-8")
        changed = original.replace("(mid 6.464466 4.464466)", "(mid 6 4)", 1)
        self.assertNotEqual(original, changed)
        with patch.object(Path, "read_text", return_value=changed):
            with self.assertRaisesRegex(AssertionError, "corner radius"):
                check_enclosure.check_board()

    def test_y_up_preserves_faces_and_volume(self):
        with TemporaryDirectory() as temporary:
            path = Path(temporary) / "rotated.stl"
            with patch.object(check_enclosure, "read_triangles", return_value=self.closed):
                check_enclosure.write_y_up(Path("source.stl"), path, 1)
            result = check_enclosure.read_triangles(path)
            expected = [[(x, z, 1 - y) for x, y, z in t] for t in self.closed]
            self.assertEqual(result, expected)
            _, volume = check_enclosure.mesh_info(path, 2)
            self.assertAlmostEqual(volume, 1 / 6)

    def test_lid_print_preserves_faces_and_volume(self):
        with TemporaryDirectory() as temporary:
            path = Path(temporary) / "lid-print.stl"
            with patch.object(check_enclosure, "read_triangles", return_value=self.closed):
                check_enclosure.write_lid_print(Path("source.stl"), path, 1.2)
            result = check_enclosure.read_triangles(path)
            expected = [[(2 - z, y - 6.5, x - 2.9) for x, y, z in t] for t in self.closed]
            self.assertEqual(len(result), len(expected))
            for actual, wanted in zip(result, expected):
                for a, b in zip(actual, wanted):
                    for x, y in zip(a, b):
                        self.assertAlmostEqual(x, y, places=6)
            _, volume = check_enclosure.mesh_info(path, 2)
            self.assertAlmostEqual(volume, 1 / 6)


if __name__ == "__main__":
    unittest.main()
