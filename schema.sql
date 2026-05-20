CREATE TABLE IF NOT EXISTS scheduled_messages
(
    id SERIAL PRIMARY KEY,
    type VARCHAR(16) NOT NULL,
    channel_id BIGINT NOT NULL,
    message_text TEXT NOT NULL,
    next_run_time TIMESTAMP WITH TIME ZONE NOT NULL,
    interval_seconds BIGINT,
    interval_str VARCHAR(128),
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_scheduled_messages_next_run ON scheduled_messages (next_run_time);