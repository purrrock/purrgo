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
