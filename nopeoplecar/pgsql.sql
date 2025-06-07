-- 启用PostGIS扩展（如果尚未安装）
CREATE EXTENSION IF NOT EXISTS postgis;
COMMENT ON EXTENSION postgis IS 'PostGIS地理空间数据处理扩展';

-- ========================
-- 无人车表（核心设备表）
-- ========================
CREATE TABLE xc_uv_vehicle (
                               uv_id BIGSERIAL,
                               uv_code VARCHAR(24) NOT NULL,
                               model_type SMALLINT NOT NULL,
                               status SMALLINT NOT NULL DEFAULT 0,
                               battery SMALLINT NOT NULL DEFAULT 100,
                               capabilities INT NOT NULL DEFAULT 0,
                               location GEOMETRY(POINT, 4326) NOT NULL,
                               version INT NOT NULL DEFAULT 0,
                               heartbeat_time TIMESTAMPTZ,
                               created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
                               updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
                               PRIMARY KEY (uv_id)
) PARTITION BY HASH(uv_id);
COMMENT ON TABLE xc_uv_vehicle IS '无人车核心信息表，记录车辆状态、位置及能力属性';
-- 字段注释
COMMENT ON COLUMN xc_uv_vehicle.uv_id IS '无人车唯一标识ID，自增长';
COMMENT ON COLUMN xc_uv_vehicle.uv_code IS '无人车编号，格式：UV+年份+序号，如UV2024-001';
COMMENT ON COLUMN xc_uv_vehicle.model_type IS '车型：1-四轮车 2-无人机 3-配送机器人';
COMMENT ON COLUMN xc_uv_vehicle.status IS '当前状态：0-离线 1-待命 2-抢单中 3-配送中 4-充电中';
COMMENT ON COLUMN xc_uv_vehicle.battery IS '剩余电量百分比，范围0-100';
COMMENT ON COLUMN xc_uv_vehicle.capabilities IS '能力位掩码：1-保温箱 2-防震 4-防水 8-夜视';
COMMENT ON COLUMN xc_uv_vehicle.location IS '当前GPS坐标(WGS84标准)，存储格式：POINT(经度 纬度)';
COMMENT ON COLUMN xc_uv_vehicle.version IS '乐观锁版本号，每次更新+1';
COMMENT ON COLUMN xc_uv_vehicle.heartbeat_time IS '最后心跳时间，超过30秒未更新视为离线';
COMMENT ON COLUMN xc_uv_vehicle.created_at IS '记录创建时间';
COMMENT ON COLUMN xc_uv_vehicle.updated_at IS '最后更新时间';


-- 无人车表分区
CREATE TABLE xc_uv_vehicle_p0 PARTITION OF xc_uv_vehicle
    FOR VALUES WITH (MODULUS 4, REMAINDER 0);
COMMENT ON TABLE xc_uv_vehicle_p0 IS '无人车表哈希分区0（MODULUS 4, REMAINDER 0），分散写入负载';

CREATE TABLE xc_uv_vehicle_p1 PARTITION OF xc_uv_vehicle
    FOR VALUES WITH (MODULUS 4, REMAINDER 1);
COMMENT ON TABLE xc_uv_vehicle_p1 IS '无人车表哈希分区1（MODULUS 4, REMAINDER 1），分散写入负载';

-- 补充分区2和3
CREATE TABLE xc_uv_vehicle_p2 PARTITION OF xc_uv_vehicle
    FOR VALUES WITH (MODULUS 4, REMAINDER 2);
COMMENT ON TABLE xc_uv_vehicle_p2 IS '无人车表哈希分区2';

CREATE TABLE xc_uv_vehicle_p3 PARTITION OF xc_uv_vehicle
    FOR VALUES WITH (MODULUS 4, REMAINDER 3);
COMMENT ON TABLE xc_uv_vehicle_p3 IS '无人车表哈希分区3';

-- 无人车表索引
CREATE INDEX idx_uv_geo ON xc_uv_vehicle USING GIST(location)
    WHERE status IN (1,2);
COMMENT ON INDEX idx_uv_geo IS '空间索引：加速查询待命/抢单中的无人车位置，使用GIST索引优化地理查询';

CREATE INDEX idx_uv_status ON xc_uv_vehicle (status, model_type, battery)
    WHERE status = 1;
COMMENT ON INDEX idx_uv_status IS '复合状态索引：快速筛选可用（状态=1）的无人车，按车型和电量过滤';

ALTER TABLE xc_uv_vehicle ADD CONSTRAINT uk_uv_code UNIQUE (uv_id, uv_code);
COMMENT ON CONSTRAINT uk_uv_code ON xc_uv_vehicle IS '唯一约束：确保无人车编号不重复';

-- ========================
-- 订单表（抢单核心表）
-- ========================
CREATE TABLE xc_uv_order (
                             order_id BIGSERIAL,
                             order_no VARCHAR(24) NOT NULL,
                             merchant_id BIGINT NOT NULL,
                             reward DECIMAL(8,2) NOT NULL,
                             required_type SMALLINT,
                             pickup GEOMETRY(POINT, 4326) NOT NULL,
                             delivery GEOMETRY(POINT, 4326) NOT NULL,
                             distance INT NOT NULL,
                             status SMALLINT NOT NULL DEFAULT 0,
                             version INT NOT NULL DEFAULT 0,
                             expire_time TIMESTAMPTZ NOT NULL,
                             uv_id BIGINT,
                             created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
                             updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
                             PRIMARY KEY (order_id,status)
) PARTITION BY LIST(status);
COMMENT ON TABLE xc_uv_order IS '配送订单表，记录抢单状态、金额及地理信息';
-- 字段注释
COMMENT ON COLUMN xc_uv_order.order_id IS '订单唯一标识ID，自增长';
COMMENT ON COLUMN xc_uv_order.order_no IS '订单编号，格式：T+年月日+序号，如T20240315-0001';
COMMENT ON COLUMN xc_uv_order.merchant_id IS '商家ID，关联商家系统';
COMMENT ON COLUMN xc_uv_order.reward IS '抢单奖励金额，单位：元';
COMMENT ON COLUMN xc_uv_order.required_type IS '要求的车型：NULL-不限制 1-四轮车 2-无人机 3-机器人';
COMMENT ON COLUMN xc_uv_order.pickup IS '取货点坐标(WGS84标准)';
COMMENT ON COLUMN xc_uv_order.delivery IS '送货点坐标(WGS84标准)';
COMMENT ON COLUMN xc_uv_order.distance IS '配送距离，单位：米';
COMMENT ON COLUMN xc_uv_order.status IS '订单状态：0-待抢单 1-已抢单 2-配送中 3-已完成 4-已取消';
COMMENT ON COLUMN xc_uv_order.version IS '乐观锁版本号，每次更新+1';
COMMENT ON COLUMN xc_uv_order.expire_time IS '抢单截止时间，超时自动取消';
COMMENT ON COLUMN xc_uv_order.uv_id IS '抢中订单的无人车ID，外键关联xc_uv_vehicle';
COMMENT ON COLUMN xc_uv_order.created_at IS '订单创建时间';
COMMENT ON COLUMN xc_uv_order.updated_at IS '订单最后更新时间';

-- 订单表分区（按状态分离热数据）
CREATE TABLE xc_uv_order_pending PARTITION OF xc_uv_order
    FOR VALUES IN (0);
COMMENT ON TABLE xc_uv_order_pending IS '待抢单分区（status=0），高频读写热点数据';

CREATE TABLE xc_uv_order_active PARTITION OF xc_uv_order
    FOR VALUES IN (1,2);
COMMENT ON TABLE xc_uv_order_active IS '已抢单/配送中分区（status=1,2），中等读写频率';

CREATE TABLE xc_uv_order_archive PARTITION OF xc_uv_order
    FOR VALUES IN (3);
COMMENT ON TABLE xc_uv_order_archive IS '已完成订单分区（status=3），只读归档数据';

-- 订单表索引
CREATE INDEX idx_order_pickup_geo ON xc_uv_order USING GIST(pickup)
    WHERE status = 0;
COMMENT ON INDEX idx_order_pickup_geo IS '空间索引：加速查询待抢单（status=0）的取货点位置';

CREATE INDEX idx_order_competition ON xc_uv_order
    (status, required_type, expire_time)
    WHERE status = 0;
COMMENT ON INDEX idx_order_competition IS '抢单竞争复合索引：优化按状态、车型要求和过期时间的查询效率';

ALTER TABLE xc_uv_order ADD CONSTRAINT uk_order_no UNIQUE (order_id, order_no, status);
COMMENT ON CONSTRAINT uk_order_no ON xc_uv_order IS '唯一约束：确保订单编号不重复';

ALTER TABLE xc_uv_order ADD CONSTRAINT fk_order_vehicle
    FOREIGN KEY (uv_id) REFERENCES xc_uv_vehicle(uv_id);
COMMENT ON CONSTRAINT fk_order_vehicle ON xc_uv_order IS '外键约束：关联抢单成功的无人车';

-- ========================
-- 抢单记录表（日志表）
-- ========================
CREATE TABLE xc_uv_grab_log (
                                log_id BIGSERIAL,
                                order_id BIGINT NOT NULL,
                                status SMALLINT NOT NULL,
                                uv_id BIGINT NOT NULL,
                                result SMALLINT NOT NULL,
                                bid_amount DECIMAL(8,2),
                                response_time INT NOT NULL,
                                created_at TIMESTAMP NOT NULL DEFAULT NOW(),  -- 改为TIMESTAMP
                                updated_at TIMESTAMP,  -- 改为TIMESTAMP
                                is_delete SMALLINT DEFAULT 0,
                                PRIMARY KEY (log_id, created_at)
) PARTITION BY RANGE (created_at);
COMMENT ON TABLE xc_uv_grab_log IS '抢单操作日志表，记录每次抢单尝试的结果和响应时间';

-- 字段注释
COMMENT ON COLUMN xc_uv_grab_log.log_id IS '日志ID，自增长';
COMMENT ON COLUMN xc_uv_grab_log.order_id IS '关联的订单ID，外键关联xc_uv_order';
COMMENT ON COLUMN xc_uv_grab_log.status IS '订单状态：0-待抢单 1-已抢单 2-配送中 3-已完成 4-已取消';
COMMENT ON COLUMN xc_uv_grab_log.uv_id IS '抢单的无人车ID，外键关联xc_uv_vehicle';
COMMENT ON COLUMN xc_uv_grab_log.result IS '抢单结果：0-失败 1-成功';
COMMENT ON COLUMN xc_uv_grab_log.bid_amount IS '实际竞价金额（用于动态定价策略）';
COMMENT ON COLUMN xc_uv_grab_log.response_time IS '响应耗时，单位：毫秒';
COMMENT ON COLUMN xc_uv_grab_log.created_at IS '记录创建时间';


ALTER TABLE xc_uv_grab_log ADD CONSTRAINT fk_grab_order
    FOREIGN KEY (order_id,status) REFERENCES xc_uv_order(order_id,status);
COMMENT ON CONSTRAINT fk_grab_order ON xc_uv_grab_log IS '外键约束：关联被抢的订单';

ALTER TABLE xc_uv_grab_log ADD CONSTRAINT fk_grab_vehicle
    FOREIGN KEY (uv_id) REFERENCES xc_uv_vehicle(uv_id);
COMMENT ON CONSTRAINT fk_grab_vehicle ON xc_uv_grab_log IS '外键约束：关联参与抢单的无人车';

-- 创建默认分区（如果表已存在但无默认分区）
CREATE TABLE xc_uv_grab_log_default PARTITION OF xc_uv_grab_log
    DEFAULT;
COMMENT ON TABLE xc_uv_grab_log_default IS '抢单日志默认分区，存储未匹配任何分区的数据';

-- 抢单日志按月分区
CREATE TABLE xc_uv_grab_log_202403 PARTITION OF xc_uv_grab_log
    FOR VALUES FROM ('2024-03-01') TO ('2024-04-01');
COMMENT ON TABLE xc_uv_grab_log_202403 IS '2024年3月抢单日志分区，便于按月归档';

-- ========================
-- 配送任务表（执行表）
-- ========================xc_uv_grab_log
CREATE TABLE xc_uv_delivery (
                                task_id BIGSERIAL,
                                order_id BIGINT NOT NULL,
                                uv_id BIGINT NOT NULL,
                                actual_distance INT,
                                start_time TIMESTAMPTZ NOT NULL,
                                end_time TIMESTAMPTZ,
                                status SMALLINT NOT NULL DEFAULT 0,
                                created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
                                PRIMARY KEY (task_id,status)  -- 包含分区键
) PARTITION BY LIST(status);
COMMENT ON TABLE xc_uv_delivery IS '配送任务执行表，记录任务状态和实际路径数据';

-- 字段注释
COMMENT ON COLUMN xc_uv_delivery.task_id IS '任务ID，自增长';
COMMENT ON COLUMN xc_uv_delivery.order_id IS '关联的订单ID，外键关联xc_uv_order';
COMMENT ON COLUMN xc_uv_delivery.uv_id IS '执行的无人车ID，外键关联xc_uv_vehicle';
COMMENT ON COLUMN xc_uv_delivery.actual_distance IS '实际行驶距离，单位：米（可能与预估距离不同）';
COMMENT ON COLUMN xc_uv_delivery.start_time IS '任务开始时间';
COMMENT ON COLUMN xc_uv_delivery.end_time IS '任务完成时间（NULL表示未完成）';
COMMENT ON COLUMN xc_uv_delivery.status IS '任务状态：0-取货中 1-配送中 2-已完成';
COMMENT ON COLUMN xc_uv_delivery.created_at IS '记录创建时间';

-- 配送任务分区
CREATE TABLE xc_uv_delivery_p0 PARTITION OF xc_uv_delivery
    FOR VALUES IN (0);
COMMENT ON TABLE xc_uv_delivery_p0 IS '取货中任务分区（status=0）';

CREATE TABLE xc_uv_delivery_p1 PARTITION OF xc_uv_delivery
    FOR VALUES IN (1);
COMMENT ON TABLE xc_uv_delivery_p1 IS '配送中任务分区（status=1）';

CREATE TABLE xc_uv_delivery_p2 PARTITION OF xc_uv_delivery
    FOR VALUES IN (2);
COMMENT ON TABLE xc_uv_delivery_p2 IS '已完成任务分区（status=2）';

ALTER TABLE xc_uv_delivery ADD CONSTRAINT uk_delivery_order
    UNIQUE (order_id, status);
COMMENT ON CONSTRAINT uk_delivery_order ON xc_uv_delivery IS '唯一约束：确保订单与任务1:1对应';

ALTER TABLE xc_uv_delivery ADD CONSTRAINT fk_delivery_order
    FOREIGN KEY (order_id,status) REFERENCES xc_uv_order(order_id,status);
COMMENT ON CONSTRAINT fk_delivery_order ON xc_uv_delivery IS '外键约束：关联原始订单';

ALTER TABLE xc_uv_delivery ADD CONSTRAINT fk_delivery_vehicle
    FOREIGN KEY (uv_id) REFERENCES xc_uv_vehicle(uv_id);
COMMENT ON CONSTRAINT fk_delivery_vehicle ON xc_uv_delivery IS '外键约束：关联执行任务的无人车';




-- 1. 无人车表 (xc_uv_vehicle)
DO $$
    BEGIN
        IF NOT EXISTS (SELECT 1 FROM information_schema.columns
                       WHERE table_name = 'xc_uv_vehicle' AND column_name = 'updated_at') THEN
            ALTER TABLE xc_uv_vehicle ADD COLUMN updated_at TIMESTAMP WITH TIME ZONE;
            COMMENT ON COLUMN xc_uv_vehicle.updated_at IS '最后更新时间';
        END IF;

        IF NOT EXISTS (SELECT 1 FROM information_schema.columns
                       WHERE table_name = 'xc_uv_vehicle' AND column_name = 'is_delete') THEN
            ALTER TABLE xc_uv_vehicle ADD COLUMN is_delete SMALLINT DEFAULT 0 NOT NULL;
            COMMENT ON COLUMN xc_uv_vehicle.is_delete IS '删除标记(0:未删除,1:已删除)';
        END IF;
    END
$$;

-- 2. 订单表 (xc_uv_order)
DO $$
    BEGIN
        IF NOT EXISTS (SELECT 1 FROM information_schema.columns
                       WHERE table_name = 'xc_uv_order' AND column_name = 'updated_at') THEN
            ALTER TABLE xc_uv_order ADD COLUMN updated_at TIMESTAMP WITH TIME ZONE;
            COMMENT ON COLUMN xc_uv_order.updated_at IS '最后更新时间';
        END IF;

        IF NOT EXISTS (SELECT 1 FROM information_schema.columns
                       WHERE table_name = 'xc_uv_order' AND column_name = 'is_delete') THEN
            ALTER TABLE xc_uv_order ADD COLUMN is_delete SMALLINT DEFAULT 0 NOT NULL;
            COMMENT ON COLUMN xc_uv_order.is_delete IS '删除标记(0:未删除,1:已删除)';
        END IF;
    END
$$;

-- 3. 抢单记录表 (xc_uv_grab_log)
DO $$
    BEGIN
        IF NOT EXISTS (SELECT 1 FROM information_schema.columns
                       WHERE table_name = 'xc_uv_grab_log' AND column_name = 'updated_at') THEN
            ALTER TABLE xc_uv_grab_log ADD COLUMN updated_at TIMESTAMP WITH TIME ZONE;
            COMMENT ON COLUMN xc_uv_grab_log.updated_at IS '最后更新时间';
        END IF;

        IF NOT EXISTS (SELECT 1 FROM information_schema.columns
                       WHERE table_name = 'xc_uv_grab_log' AND column_name = 'is_delete') THEN
            ALTER TABLE xc_uv_grab_log ADD COLUMN is_delete SMALLINT DEFAULT 0 NOT NULL;
            COMMENT ON COLUMN xc_uv_grab_log.is_delete IS '删除标记(0:未删除,1:已删除)';
        END IF;
    END
$$;

-- 4. 配送任务表 (xc_uv_delivery)
DO $$
    BEGIN
        IF NOT EXISTS (SELECT 1 FROM information_schema.columns
                       WHERE table_name = 'xc_uv_delivery' AND column_name = 'updated_at') THEN
            ALTER TABLE xc_uv_delivery ADD COLUMN updated_at TIMESTAMP WITH TIME ZONE;
            COMMENT ON COLUMN xc_uv_delivery.updated_at IS '最后更新时间';
        END IF;

        IF NOT EXISTS (SELECT 1 FROM information_schema.columns
                       WHERE table_name = 'xc_uv_delivery' AND column_name = 'is_delete') THEN
            ALTER TABLE xc_uv_delivery ADD COLUMN is_delete SMALLINT DEFAULT 0 NOT NULL;
            COMMENT ON COLUMN xc_uv_delivery.is_delete IS '删除标记(0:未删除,1:已删除)';
        END IF;
    END
$$;


-- 1. 订单表添加订单类型字段 (xc_uv_order)
DO $$
    BEGIN
        IF NOT EXISTS (SELECT 1 FROM information_schema.columns
                       WHERE table_name = 'xc_uv_order' AND column_name = 'order_type') THEN
            ALTER TABLE xc_uv_order ADD COLUMN order_type VARCHAR(20) NOT NULL DEFAULT 'daily_goods';
            COMMENT ON COLUMN xc_uv_order.order_type IS '订单类型(daily_goods,food,medicine,delivery)';

            -- 添加索引提高查询效率
            CREATE INDEX idx_xc_uv_order_type ON xc_uv_order(order_type);
        END IF;
    END
$$;

-- 2. 无人车表添加支持订单类型字段 (xc_uv_vehicle)
DO $$
    BEGIN
        IF NOT EXISTS (SELECT 1 FROM information_schema.columns
                       WHERE table_name = 'xc_uv_vehicle' AND column_name = 'supported_types') THEN
            ALTER TABLE xc_uv_vehicle ADD COLUMN supported_types VARCHAR(100) NOT NULL DEFAULT 'daily_goods';
            COMMENT ON COLUMN xc_uv_vehicle.supported_types IS '支持订单类型(逗号分隔:daily_goods,food,medicine,delivery)';
        END IF;
    END
$$;


-- 修改字段类型为字符串类型（推荐TEXT以适应长内容）
ALTER TABLE public.xc_uv_vehicle
    ALTER COLUMN capabilities TYPE TEXT;

-- 更新注释说明
COMMENT ON COLUMN public.xc_uv_vehicle.capabilities IS '能力列表，逗号分隔的字符串，如：保温箱,防震';


SELECT
    child.relname AS partition_name,
    pg_get_expr(child.relpartbound, child.oid) AS partition_bound,
    pg_size_pretty(pg_total_relation_size(child.oid)) AS partition_size
FROM pg_inherits
         JOIN pg_class parent ON inhparent = parent.oid
         JOIN pg_class child ON inhrelid = child.oid
         JOIN pg_namespace nmsp_parent ON nmsp_parent.oid = parent.relnamespace
         JOIN pg_namespace nmsp_child ON nmsp_child.oid = child.relnamespace
         JOIN pg_partitioned_table pt ON pt.partrelid = parent.oid
WHERE parent.relname = 'xc_uv_grab_log'
ORDER BY partition_bound;

-- 查看分区定义
SELECT relname, pg_get_expr(relpartbound, oid)
FROM pg_class
WHERE relname = 'xc_uv_grab_log_202301';

-- 从主表分离错误分区（保留数据）
ALTER TABLE xc_uv_grab_log DETACH PARTITION xc_uv_grab_log_202301;

-- 彻底删除分区表
DROP TABLE IF EXISTS xc_uv_grab_log_202301;


-- xc_uv_vehicle
ALTER TABLE xc_uv_vehicle
    ALTER COLUMN created_at TYPE timestamp(6),
    ALTER COLUMN updated_at TYPE timestamp(6),
    ALTER COLUMN heartbeat_time TYPE timestamp(6);

-- xc_uv_order
ALTER TABLE xc_uv_order
    ALTER COLUMN created_at TYPE timestamp(6),
    ALTER COLUMN updated_at TYPE timestamp(6),
    ALTER COLUMN expire_time TYPE timestamp(6);

-- xc_uv_grab_log
ALTER TABLE xc_uv_grab_log
    ALTER COLUMN created_at TYPE timestamp(6),
    ALTER COLUMN updated_at TYPE timestamp(6);

-- xc_uv_delivery
ALTER TABLE xc_uv_delivery
    ALTER COLUMN created_at TYPE timestamp(6),
    ALTER COLUMN updated_at TYPE timestamp(6),
    ALTER COLUMN start_time TYPE timestamp(6),
    ALTER COLUMN end_time TYPE timestamp(6);


-- 1. 移除分区键依赖（必须步骤）
ALTER TABLE xc_uv_grab_log DETACH PARTITION xc_uv_grab_log_default;  -- 先分离所有分区

-- 2. 修改字段类型（直接操作）
ALTER TABLE xc_uv_grab_log
    ALTER COLUMN created_at TYPE timestamp(6) USING created_at::timestamp,
    ALTER COLUMN updated_at TYPE timestamp(6) USING updated_at::timestamp;

-- 3. 重建分区策略
ALTER TABLE xc_uv_grab_log ATTACH PARTITION xc_uv_grab_log_default DEFAULT;