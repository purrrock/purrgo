import unittest
import struct
from purrgo_models import MapFeature, RTreeNode, HWConfig

class TestMapCompilerModels(unittest.TestCase):

    def test_data_node_size(self):
        """Test that Data Node packing is exactly 25 bytes and format is correct."""
        feature = MapFeature(
            osm_id="123",
            code=42,
            name="Test",
            points=b'',
            lod=1
        )
        feature.bbox = (10, 20, 30, 40)
        feature.v1 = 100
        feature.v2 = 200

        packed = feature.pack_data_node()
        self.assertEqual(len(packed), 25, f"Data Node should be 25 bytes, got {len(packed)}")

        # Verify unpack format
        unpacked = struct.unpack("<iiiiBII", packed)
        self.assertEqual(unpacked[0], 10)
        self.assertEqual(unpacked[1], 20)
        self.assertEqual(unpacked[2], 30)
        self.assertEqual(unpacked[3], 40)
        self.assertEqual(unpacked[4], 42)
        self.assertEqual(unpacked[5], 100)
        self.assertEqual(unpacked[6], 200)

    def test_rtree_node_level_0_v3_jump(self):
        """Test v3_jump calculation for Level 0 nodes (children are Data Nodes)."""
        f1 = MapFeature(osm_id="1", code=1, name="", points=b'', lod=0)
        f1.bbox = (0, 0, 10, 10)
        f2 = MapFeature(osm_id="2", code=2, name="", points=b'', lod=0)
        f2.bbox = (10, 10, 20, 20)

        node = RTreeNode(level=0, children=[f1, f2])

        # At Level 0, children are Data Nodes, size of each is HWConfig.DATA_NODE_SIZE (25 bytes)
        expected_v3_jump = 2 * 25
        # The node itself is a Nav Node, size HWConfig.NAV_NODE_SIZE (28 bytes)
        expected_bin_size = 28 + expected_v3_jump

        self.assertEqual(node.v3_jump, expected_v3_jump, "Level 0 v3_jump should equal sum of children Data Node sizes without padding.")
        self.assertEqual(node.bin_size, expected_bin_size, "Level 0 bin_size should equal NAV_NODE_SIZE + v3_jump.")
        self.assertEqual(node.bbox, (0, 0, 20, 20))

    def test_rtree_node_level_1_v3_jump(self):
        """Test v3_jump calculation for Level 1 nodes (children are Level 0 RTreeNodes)."""
        f1 = MapFeature(osm_id="1", code=1, name="", points=b'', lod=0)
        f1.bbox = (0, 0, 10, 10)
        f2 = MapFeature(osm_id="2", code=2, name="", points=b'', lod=0)
        f2.bbox = (10, 10, 20, 20)
        child1 = RTreeNode(level=0, children=[f1, f2]) # bin_size = 28 + 50 = 78

        f3 = MapFeature(osm_id="3", code=3, name="", points=b'', lod=0)
        f3.bbox = (20, 20, 30, 30)
        child2 = RTreeNode(level=0, children=[f3]) # bin_size = 28 + 25 = 53

        root = RTreeNode(level=1, children=[child1, child2])

        expected_v3_jump = child1.bin_size + child2.bin_size # 78 + 53 = 131
        expected_bin_size = 28 + expected_v3_jump # 28 + 131 = 159

        self.assertEqual(root.v3_jump, expected_v3_jump, "Level 1 v3_jump should equal sum of children node sizes without padding.")
        self.assertEqual(root.bin_size, expected_bin_size, "Level 1 bin_size should equal NAV_NODE_SIZE + v3_jump.")

    def test_empty_rtree_node(self):
        """Test edge case when a node has no children."""
        node = RTreeNode(level=0, children=[])
        self.assertEqual(node.v3_jump, 0)
        self.assertEqual(node.bin_size, 28)

if __name__ == '__main__':
    unittest.main()
