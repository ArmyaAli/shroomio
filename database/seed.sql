-- Shroomio seed data
-- DML only; keep schema definitions in schema.sql.

INSERT INTO mushroom_species (
    species_id,
    name,
    description,
    pattern_id,
    rarity_tier,
    cap_color_rgba,
    unlocked_by_default,
    sort_order
) VALUES
    (0, 'Amanita muscaria', 'Red cap with white spots; iconic fairy-tale mushroom.', 0, 0, 0xE8443CFF, 1, 0),
    (1, 'Chanterelle', 'Golden funnel cap; prized edible forest mushroom.', 1, 0, 0xF2B84BFF, 1, 1),
    (2, 'Morel', 'Honeycomb cap; spring mushroom with a rugged silhouette.', 2, 1, 0xB47B48FF, 1, 2),
    (3, 'Shiitake', 'Brown cap; classic cultivated mushroom with bold gills.', 3, 0, 0x8A5A3BFF, 1, 3),
    (4, 'Oyster', 'Layered fan caps; soft clustered shelf mushroom.', 4, 0, 0xD8D2C4FF, 1, 4),
    (5, 'Enoki', 'Tiny pale caps and long stems; delicate clustered look.', 5, 1, 0xF4E8C1FF, 1, 5),
    (6, 'Portobello', 'Broad brown cap; sturdy heavyweight arena profile.', 6, 0, 0x6F4A35FF, 1, 6),
    (7, 'Lion''s Mane', 'Shaggy white spines; fluffy pom-pom silhouette.', 7, 1, 0xEFE7D6FF, 1, 7),
    (8, 'Reishi', 'Glossy red bracket; dramatic medicinal shelf form.', 8, 2, 0xB8322EFF, 1, 8),
    (9, 'Wood Blewit', 'Violet woodland cap; rare purple accent species.', 9, 2, 0x8A68B8FF, 1, 9)
ON CONFLICT(species_id) DO UPDATE SET
    name = excluded.name,
    description = excluded.description,
    pattern_id = excluded.pattern_id,
    rarity_tier = excluded.rarity_tier,
    cap_color_rgba = excluded.cap_color_rgba,
    unlocked_by_default = excluded.unlocked_by_default,
    sort_order = excluded.sort_order;
