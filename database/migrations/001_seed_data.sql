-- Shroomio Seed Data
-- Description: Sample data for testing and development

-- Insert sample players
INSERT INTO players (player_uuid, display_name, total_sessions, total_play_time_seconds) VALUES
    ('550e8400-e29b-41d4-a716-446655440001', 'MushroomKing', 15, 4500),
    ('550e8400-e29b-41d4-a716-446655440002', 'SporeHunter', 23, 6900),
    ('550e8400-e29b-41d4-a716-446655440003', 'FungiMaster', 8, 2400),
    ('550e8400-e29b-41d4-a716-446655440004', 'ToadstoolTitan', 31, 9300),
    ('550e8400-e29b-41d4-a716-446655440005', 'MycoMarauder', 12, 3600);

-- Insert sample sessions
INSERT INTO sessions (session_uuid, started_at, ended_at, duration_seconds, player_count, bot_count, status) VALUES
    ('660e8400-e29b-41d4-a716-446655440001', '2026-06-10T14:30:00Z', '2026-06-10T15:00:00Z', 1800, 5, 18, 'completed'),
    ('660e8400-e29b-41d4-a716-446655440002', '2026-06-10T16:00:00Z', '2026-06-10T16:45:00Z', 2700, 4, 18, 'completed'),
    ('660e8400-e29b-41d4-a716-446655440003', '2026-06-11T10:00:00Z', NULL, NULL, 3, 18, 'active');

-- Insert session participants
INSERT INTO session_participants (session_id, player_id, joined_at, left_at, final_rank, final_mass) VALUES
    (1, 1, '2026-06-10T14:30:00Z', '2026-06-10T15:00:00Z', 1, 2450.5),
    (1, 2, '2026-06-10T14:30:00Z', '2026-06-10T14:55:00Z', 3, 890.2),
    (1, 3, '2026-06-10T14:32:00Z', '2026-06-10T14:48:00Z', 4, 450.0),
    (1, 4, '2026-06-10T14:30:00Z', '2026-06-10T15:00:00Z', 2, 1820.8),
    (1, 5, '2026-06-10T14:35:00Z', '2026-06-10T14:42:00Z', 5, 280.3),
    (2, 1, '2026-06-10T16:00:00Z', '2026-06-10T16:45:00Z', 2, 1650.4),
    (2, 2, '2026-06-10T16:00:00Z', '2026-06-10T16:45:00Z', 1, 2100.7),
    (2, 3, '2026-06-10T16:05:00Z', '2026-06-10T16:30:00Z', 3, 720.1),
    (2, 4, '2026-06-10T16:00:00Z', '2026-06-10T16:20:00Z', 4, 380.9),
    (3, 1, '2026-06-11T10:00:00Z', NULL, NULL, NULL),
    (3, 2, '2026-06-11T10:00:00Z', NULL, NULL, NULL),
    (3, 5, '2026-06-11T10:02:00Z', NULL, NULL, NULL);

-- Insert player statistics
INSERT INTO player_stats (player_id, total_games_played, total_kills, total_deaths, total_mass_consumed, total_mass_lost, total_distance_traveled, total_spores_consumed, total_players_consumed, highest_mass_achieved, highest_rank_achieved, longest_survival_seconds) VALUES
    (1, 15, 42, 18, 15680.5, 8920.3, 45600.0, 320, 42, 2450.5, 1, 1800.0),
    (2, 23, 67, 25, 24500.8, 12450.6, 68900.0, 485, 67, 2100.7, 1, 2700.0),
    (3, 8, 12, 10, 4200.2, 3800.1, 22400.0, 180, 12, 720.1, 3, 1500.0),
    (4, 31, 89, 35, 38900.4, 18200.7, 95200.0, 720, 89, 1820.8, 1, 2400.0),
    (5, 12, 18, 15, 6800.6, 5400.2, 33600.0, 240, 18, 280.3, 4, 900.0);

-- Insert sample match events
INSERT INTO match_events (session_id, event_type, event_timestamp, tick_number, actor_player_id, target_player_id, mass_value, position_x, position_y) VALUES
    (1, 'player_spawn', '2026-06-10T14:30:00Z', 0, 1, NULL, 100.0, 1500.0, 1500.0),
    (1, 'player_spawn', '2026-06-10T14:30:00Z', 0, 2, NULL, 100.0, 2000.0, 1800.0),
    (1, 'player_consume_spore', '2026-06-10T14:35:00Z', 150, 1, NULL, 8.0, 1520.0, 1510.0),
    (1, 'player_consume_spore', '2026-06-10T14:35:30Z', 165, 2, NULL, 8.0, 2010.0, 1805.0),
    (1, 'player_consume_player', '2026-06-10T14:42:00Z', 720, 1, 5, 280.3, 1600.0, 1550.0),
    (1, 'player_death', '2026-06-10T14:42:00Z', 720, 5, 1, 280.3, 1600.0, 1550.0),
    (1, 'player_reach_mass', '2026-06-10T14:50:00Z', 1200, 1, NULL, 1000.0, 1650.0, 1600.0),
    (1, 'player_consume_player', '2026-06-10T14:55:00Z', 1500, 2, 3, 450.0, 2100.0, 1850.0),
    (1, 'player_death', '2026-06-10T14:55:00Z', 1500, 3, 2, 450.0, 2100.0, 1850.0),
    (2, 'player_spawn', '2026-06-10T16:00:00Z', 0, 1, NULL, 100.0, 1200.0, 1200.0),
    (2, 'player_spawn', '2026-06-10T16:00:00Z', 0, 2, NULL, 100.0, 2500.0, 2500.0),
    (2, 'player_consume_player', '2026-06-10T16:30:00Z', 1800, 2, 3, 720.1, 2600.0, 2550.0),
    (2, 'player_death', '2026-06-10T16:30:00Z', 1800, 3, 2, 720.1, 2600.0, 2550.0);
