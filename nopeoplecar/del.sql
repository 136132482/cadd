SELECT * FROM pg_extension WHERE extname = 'postgis';


-- 删除顺序：先删除有外键依赖的表，最后删除被依赖的表
-- 1. 先删除配送任务表（依赖订单表和无人车表）
DROP TABLE IF EXISTS xc_uv_delivery_p2 CASCADE;
DROP TABLE IF EXISTS xc_uv_delivery_p1 CASCADE;
DROP TABLE IF EXISTS xc_uv_delivery_p0 CASCADE;
DROP TABLE IF EXISTS xc_uv_delivery CASCADE;

-- 2. 删除抢单记录表（依赖订单表和无人车表）
DROP TABLE IF EXISTS xc_uv_grab_log_202403 CASCADE;
DROP TABLE IF EXISTS xc_uv_grab_log CASCADE;

-- 3. 删除订单表（依赖无人车表）
DROP TABLE IF EXISTS xc_uv_order_archive CASCADE;
DROP TABLE IF EXISTS xc_uv_order_active CASCADE;
DROP TABLE IF EXISTS xc_uv_order_pending CASCADE;
DROP TABLE IF EXISTS xc_uv_order CASCADE;

-- 4. 最后删除无人车表
DROP TABLE IF EXISTS xc_uv_vehicle_p1 CASCADE;
DROP TABLE IF EXISTS xc_uv_vehicle_p0 CASCADE;
DROP TABLE IF EXISTS xc_uv_vehicle CASCADE;
