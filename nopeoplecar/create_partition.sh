#!/bin/bash

PSQL="psql -U pg_db -d your_database -qAt"

$PSQL <<EOF
DO $$
    BEGIN
        -- 检查触发器是否存在
        IF EXISTS (
            SELECT 1 FROM pg_trigger
            WHERE tgname = 'tr_auto_partition'
              AND tgrelid = 'xc_uv_grab_log'::regclass
        ) THEN
            -- 删除旧触发器
            EXECUTE 'DROP TRIGGER tr_auto_partition ON xc_uv_grab_log';
            RAISE NOTICE '已删除现有触发器 tr_auto_partition';
        END IF;

        -- 创建新触发器
        EXECUTE '
    CREATE TRIGGER tr_auto_partition
    BEFORE INSERT ON xc_uv_grab_log
    FOR EACH ROW EXECUTE FUNCTION create_partition_on_insert()';

        RAISE NOTICE '成功创建自动分区触发器';
    EXCEPTION WHEN OTHERS THEN
        RAISE EXCEPTION '触发器创建失败: %', SQLERRM;
    END
$$;
EOF

$PSQL <<EOF
CREATE OR REPLACE FUNCTION public.create_partition_on_insert()
    RETURNS TRIGGER AS $$
DECLARE
    partition_name TEXT;
    partition_start TEXT;
    partition_end TEXT;
    retry_count INT := 0;
    max_retries INT := 3;
BEGIN
    -- 生成分区名（格式：xc_uv_grab_log_YYYYMM）
    partition_name := 'xc_uv_grab_log_' || TO_CHAR(NEW.created_at, 'YYYYMM');

    -- 重试机制（应对并发创建）
    WHILE retry_count < max_retries LOOP
            BEGIN
                -- 检查分区是否存在（使用pg_class系统目录，性能更好）
                IF NOT EXISTS (
                    SELECT 1 FROM pg_class c
                                      JOIN pg_namespace n ON c.relnamespace = n.oid
                    WHERE c.relname = partition_name
                      AND n.nspname = 'public'
                      AND c.relkind = 'r'
                ) THEN
                    -- 计算分区时间范围
                    partition_start := TO_CHAR(DATE_TRUNC('MONTH', NEW.created_at), 'YYYY-MM-DD');
                    partition_end := TO_CHAR(
                            DATE_TRUNC('MONTH', NEW.created_at) + INTERVAL '1 MONTH',
                            'YYYY-MM-DD'
                                     );

                    -- 动态创建分区（带并发冲突处理）
                    EXECUTE format('
          CREATE TABLE IF NOT EXISTS %I PARTITION OF xc_uv_grab_log
          FOR VALUES FROM (%L) TO (%L)',
                                   partition_name, partition_start, partition_end
                            );

                    -- 添加分区注释
                    EXECUTE format('
          COMMENT ON TABLE %I IS %L',
                                   partition_name,
                                   TO_CHAR(NEW.created_at, '"YYYY年MM月"') || '抢单日志分区'
                            );

                    RAISE NOTICE '自动创建分区: %', partition_name;
                END IF;

                -- 成功则退出循环
                EXIT;
            EXCEPTION WHEN OTHERS THEN
                retry_count := retry_count + 1;
                IF retry_count >= max_retries THEN
                    RAISE EXCEPTION '创建分区失败: %', SQLERRM;
                END IF;

                -- 等待随机时间避免并发冲突
                PERFORM pg_sleep(random() * 0.1);
            END;
        END LOOP;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;
EOF

$PSQL <<EOF
CREATE TRIGGER tr_auto_partition
BEFORE INSERT ON xc_uv_grab_log
FOR EACH ROW EXECUTE FUNCTION create_partition_on_insert();
EOF

echo "自动分区触发器部署完成"
