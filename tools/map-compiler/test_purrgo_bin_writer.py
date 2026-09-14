import unittest
import struct
import os
from purrgo_bin_writer import MapCompiler
from purrgo_models import HWConfig

class TestMapCompilerBinWriter(unittest.TestCase):
    def test_write_pgo_container_idx(self):
        filepath = "test_write.idx"
        payload = b"IDX_PAYLOAD"
        lod_offsets = (100, 200, 300)

        MapCompiler._write_pgo_container(filepath, payload, file_type=1, lod_offsets=lod_offsets)

        with open(filepath, "rb") as f:
            header = f.read(HWConfig.PGO_HEADER_SIZE)
            read_payload = f.read()

        self.assertEqual(len(header), 32)
        self.assertEqual(header[0:3], b"PGO")
        self.assertEqual(header[3], 1)
        self.assertEqual(struct.unpack("<I", header[4:8])[0], len(payload))
        self.assertEqual(struct.unpack("<I", header[8:12])[0], 100)
        self.assertEqual(struct.unpack("<I", header[12:16])[0], 200)
        self.assertEqual(struct.unpack("<I", header[16:20])[0], 300)
        self.assertEqual(struct.unpack("<I", header[20:24])[0], 0)
        self.assertEqual(struct.unpack("<I", header[24:28])[0], 0)
        self.assertEqual(struct.unpack("<I", header[28:32])[0], 0)

        self.assertEqual(read_payload, payload)
        os.remove(filepath)

    def test_write_pgo_container_mlp(self):
        filepath = "test_write.mlp"
        payload = b"MLP_PAYLOAD"

        MapCompiler._write_pgo_container(filepath, payload, file_type=2, lod_offsets=(0, 0, 0))

        with open(filepath, "rb") as f:
            header = f.read(HWConfig.PGO_HEADER_SIZE)

        self.assertEqual(len(header), 32)
        self.assertEqual(header[0:3], b"PGO")
        self.assertEqual(header[3], 2)
        self.assertEqual(struct.unpack("<I", header[4:8])[0], len(payload))
        self.assertEqual(struct.unpack("<I", header[8:12])[0], 0)

        os.remove(filepath)

    def test_write_pgo_container_db(self):
        filepath = "test_write.db"
        payload = b"DB_PAYLOAD"

        MapCompiler._write_pgo_container(filepath, payload, file_type=3, lod_offsets=(0, 0, 0))

        with open(filepath, "rb") as f:
            header = f.read(HWConfig.PGO_HEADER_SIZE)

        self.assertEqual(len(header), 32)
        self.assertEqual(header[0:3], b"PGO")
        self.assertEqual(header[3], 3)
        self.assertEqual(struct.unpack("<I", header[4:8])[0], len(payload))
        self.assertEqual(struct.unpack("<I", header[8:12])[0], 0)

        os.remove(filepath)

    def test_spatial_order_and_serialization(self):
        from purrgo_models import MapFeature

        # Create features out of spatial order
        f1 = MapFeature(osm_id="1", code=1, name="One", points=struct.pack("<ii", 100, 100), lod=0)
        f1.bbox = (100, 100, 100, 100)
        f2 = MapFeature(osm_id="2", code=2, name="Two", points=struct.pack("<ii", 10, 10), lod=0)
        f2.bbox = (10, 10, 10, 10)
        f3 = MapFeature(osm_id="3", code=3, name="Three", points=struct.pack("<ii", 200, 200), lod=0)
        f3.bbox = (200, 200, 200, 200)

        features = [f1, f2, f3]

        # Build spatial order in-place
        MapCompiler.build_spatial_order(features)

        # Since we use STR with CHUNK_SIZE=14, for 3 elements it should sort them globally
        # Center lon/lat sorting will place them in order: f2, f1, f3
        self.assertEqual(features[0].osm_id, "2")
        self.assertEqual(features[1].osm_id, "1")
        self.assertEqual(features[2].osm_id, "3")

        # Compile to MLP and DB
        mlp_path = "test_spatial.mlp"
        db_path = "test_spatial.db"

        MapCompiler.compile_mlp(features, mlp_path)
        MapCompiler.compile_db(features, db_path)

        # Check v1 and v2 assignments
        self.assertEqual(features[0].v2, 2)  # v2 starts at 2 for non-poi
        self.assertEqual(features[1].v2, 3)
        self.assertEqual(features[2].v2, 4)

        self.assertEqual(features[0].v1, 8)  # v1 starts at 8
        self.assertTrue(features[1].v1 > features[0].v1)
        self.assertTrue(features[2].v1 > features[1].v1)

        # Check MLP binary size and locations roughly
        with open(mlp_path, "rb") as f:
            header = f.read(HWConfig.PGO_HEADER_SIZE)
            self.assertEqual(header[3], 2)
            payload = f.read()
            self.assertTrue(len(payload) > 0)

        # Check DB binary format
        with open(db_path, "rb") as f:
            header = f.read(HWConfig.PGO_HEADER_SIZE)
            self.assertEqual(header[3], 3)
            db_payload = f.read()
            self.assertTrue(len(db_payload) > 0)

        os.remove(mlp_path)
        os.remove(db_path)

    def test_create_empty_layer(self):
        prefix = "test_empty"
        MapCompiler.create_empty_layer(prefix)

        # Check MLP
        with open(f"{prefix}.mlp", "rb") as f:
            mlp_header = f.read(HWConfig.PGO_HEADER_SIZE)
            mlp_payload = f.read()
        self.assertEqual(len(mlp_header), 32)
        self.assertEqual(mlp_header[3], 2)
        self.assertEqual(struct.unpack("<I", mlp_header[4:8])[0], 0)
        self.assertEqual(mlp_payload, b"")

        # Check IDX
        with open(f"{prefix}.idx", "rb") as f:
            idx_header = f.read(HWConfig.PGO_HEADER_SIZE)
            idx_payload = f.read()
        self.assertEqual(len(idx_header), 32)
        self.assertEqual(idx_header[3], 1)
        # 3 SQT blocks of 16 bytes each
        self.assertEqual(struct.unpack("<I", idx_header[4:8])[0], 48)
        self.assertEqual(struct.unpack("<I", idx_header[8:12])[0], 32)
        self.assertEqual(struct.unpack("<I", idx_header[12:16])[0], 32 + 16)
        self.assertEqual(struct.unpack("<I", idx_header[16:20])[0], 32 + 32)
        self.assertEqual(idx_payload, (b'SQT\x01\x01\x00\x00\x00' + struct.pack("<II", 0, 0)) * 3)

        os.remove(f"{prefix}.mlp")
        os.remove(f"{prefix}.idx")

if __name__ == '__main__':
    unittest.main()
