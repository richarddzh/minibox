import unittest
from pathlib import Path
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


if __name__ == "__main__":
    unittest.main()
