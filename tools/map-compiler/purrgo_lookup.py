import sys
import csv
from typing import Dict, Tuple, List, Optional
from dataclasses import dataclass

@dataclass
class FeatureRule:
    code: int
    pg_class: str
    style: str
    lod: int
    layer: str
    osm_tags: List[Tuple[str, str]]
    icon: str


class LookupTables:
    """
    Rule table loaded from features.csv. Processes rules strictly top-to-bottom.
    """
    RULES: List[FeatureRule] = []

    @classmethod
    def load_from_csv(cls, filepath: str = "features.csv") -> None:
        """
        Parses features.csv and builds the rule list.
        """
        print(f"[>] Loading rules from {filepath}...")
        cls.RULES = []

        try:
            with open(filepath, mode="r", encoding="utf-8") as f:
                reader = csv.reader(f, delimiter=";")
                next(reader, None)  # Skip header

                loaded_records = 0
                for row in reader:
                    # Expecting at least 8 columns: Code, PG_class, STYLE, LOD, Layer, OSM_Tags, Description, Enabled
                    if len(row) < 8:
                        continue

                    try:
                        code = int(row[0].strip())
                        lod = int(row[3].strip())
                    except ValueError:
                        continue

                    enabled = row[7].strip()
                    if enabled not in ("1", "true", "yes", "on"):
                        continue

                    pg_class = row[1].strip()
                    style = row[2].strip()
                    layer = row[4].strip()

                    osm_tags_str = row[5].strip()
                    osm_tags = []
                    if osm_tags_str:
                        for tag_pair in osm_tags_str.split(","):
                            if "=" in tag_pair:
                                k, v = tag_pair.split("=", 1)
                                osm_tags.append((k.strip(), v.strip()))

                    icon = row[8].strip() if len(row) > 8 else ""

                    rule = FeatureRule(
                        code=code,
                        pg_class=pg_class,
                        style=style,
                        lod=lod,
                        layer=layer,
                        osm_tags=osm_tags,
                        icon=icon
                    )
                    cls.RULES.append(rule)
                    loaded_records += 1

            print(f"    Successfully imported rules: {loaded_records}")

        except FileNotFoundError:
            print(f"[-] Error: Configuration file {filepath} not found.")
            sys.exit(1)
        except Exception as e:
            print(f"[-] Critical error parsing {filepath}: {e}")
            sys.exit(1)

    @classmethod
    def match_feature(cls, tags: Dict[str, str]) -> Optional[FeatureRule]:
        """
        Matches tags against rules top-to-bottom. First fully matching rule wins.
        """
        if not tags:
            return None

        for rule in cls.RULES:
            match = True
            for k, v in rule.osm_tags:
                if tags.get(k) != v:
                    match = False
                    break
            if match and rule.osm_tags:
                return rule
        return None
