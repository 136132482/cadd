-- 启用必要扩展（确保有权限）
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";
CREATE EXTENSION IF NOT EXISTS "pgcrypto"; -- 必须启用此扩展才能使用加密函数

-- 用户表（使用标准密码存储方案）
CREATE TABLE users (
                       id SERIAL PRIMARY KEY,
                       username VARCHAR(50) NOT NULL UNIQUE,
                       email VARCHAR(100) NOT NULL UNIQUE,
                       password VARCHAR(255) NOT NULL, -- 存储加密后的密码
                       status INTEGER DEFAULT 1,
                       created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
                       updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

-- 商品分类表
CREATE TABLE categories (
                            id SERIAL PRIMARY KEY,
                            name VARCHAR(50) NOT NULL,
                            created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
                            updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

-- 商品表
CREATE TABLE products (
                          id SERIAL PRIMARY KEY,
                          name VARCHAR(100) NOT NULL,
                          price DECIMAL(10,2) NOT NULL CHECK (price > 0),
                          category_id INTEGER REFERENCES categories(id),
                          stock INTEGER DEFAULT 0 CHECK (stock >= 0),
                          created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
                          updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

-- 订单表
CREATE TABLE orders (
                        id SERIAL PRIMARY KEY,
                        user_id INTEGER REFERENCES users(id),
                        total_amount DECIMAL(12,2) NOT NULL CHECK (total_amount > 0),
                        status INTEGER DEFAULT 0,
                        created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
                        updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

-- 订单详情表
CREATE TABLE order_details (
                               order_id INTEGER REFERENCES orders(id) ON DELETE CASCADE,
                               product_id INTEGER REFERENCES products(id),
                               quantity INTEGER NOT NULL CHECK (quantity > 0),
                               unit_price DECIMAL(10,2) NOT NULL,
                               created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
                               updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
                               PRIMARY KEY (order_id, product_id)
);

-- 自动更新时间戳的函数
CREATE OR REPLACE FUNCTION update_timestamp()
    RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = CURRENT_TIMESTAMP;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- 为用户表创建触发器
CREATE TRIGGER tr_users_update
    BEFORE UPDATE ON users
    FOR EACH ROW EXECUTE FUNCTION update_timestamp();

-- 为分类表创建触发器
CREATE TRIGGER tr_categories_update
    BEFORE UPDATE ON categories
    FOR EACH ROW EXECUTE FUNCTION update_timestamp();

-- 为商品表创建触发器
CREATE TRIGGER tr_products_update
    BEFORE UPDATE ON products
    FOR EACH ROW EXECUTE FUNCTION update_timestamp();

-- 为订单表创建触发器
CREATE TRIGGER tr_orders_update
    BEFORE UPDATE ON orders
    FOR EACH ROW EXECUTE FUNCTION update_timestamp();

-- 为订单详情表创建触发器
CREATE TRIGGER tr_order_details_update
    BEFORE UPDATE ON order_details
    FOR EACH ROW EXECUTE FUNCTION update_timestamp();

-- 插入初始化数据（使用替代密码方案）
INSERT INTO categories (name) VALUES
                                  ('电子产品'), ('服装'), ('食品');

-- 使用MD5哈希作为示例（生产环境应使用更安全的加密方式）
INSERT INTO users (username, email, password) VALUES
                                                  ('user1', 'user1@example.com', md5('password1')),
                                                  ('user2', 'user2@example.com', md5('password2'));

INSERT INTO products (name, price, category_id, stock) VALUES
                                                           ('智能手机', 5999.99, 1, 100),
                                                           ('笔记本电脑', 8999.50, 1, 50),
                                                           ('纯棉T恤', 99.00, 2, 200);

-- 创建视图（匹配OrderDetail结构体）
CREATE OR REPLACE VIEW v_order_details AS
SELECT
    od.order_id,
    u.username,
    od.quantity * od.unit_price AS amount,
    od.created_at,
    od.updated_at
FROM order_details od
         JOIN orders o ON od.order_id = o.id
         JOIN users u ON o.user_id = u.id;

-- 设置数据库时区（使用当前数据库名称）
DO $$
    BEGIN
        EXECUTE format('ALTER DATABASE %I SET timezone TO ''UTC''', current_database());
    END
$$;







-- 1. 首先确保已定义触发器函数（只需定义一次）
CREATE OR REPLACE FUNCTION update_timestamp()
    RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = CURRENT_TIMESTAMP;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- 2. 批量创建触发器（自动覆盖所有表）
DO $$
    DECLARE
        table_record RECORD;
    BEGIN
        FOR table_record IN
            SELECT table_name
            FROM information_schema.tables
            WHERE table_schema = 'public'
              AND table_type = 'BASE TABLE'
              -- 排除不需要触发器的表（可选）
              AND table_name NOT IN ('migrations', 'system_logs')
            LOOP
                EXECUTE format('
            DROP TRIGGER IF EXISTS tr_%s_update ON %I;
            CREATE TRIGGER tr_%s_update
            BEFORE UPDATE ON %I
            FOR EACH ROW EXECUTE FUNCTION update_timestamp()',
                               table_record.table_name,
                               table_record.table_name,
                               table_record.table_name,
                               table_record.table_name
                        );
            END LOOP;
    END
$$;



-- 为所有表添加is_delete字段（默认0表示未删除）
ALTER TABLE users ADD COLUMN is_delete SMALLINT DEFAULT 0 CHECK (is_delete IN (0,1));
ALTER TABLE categories ADD COLUMN is_delete SMALLINT DEFAULT 0 CHECK (is_delete IN (0,1));
ALTER TABLE products ADD COLUMN is_delete SMALLINT DEFAULT 0 CHECK (is_delete IN (0,1));
ALTER TABLE orders ADD COLUMN is_delete SMALLINT DEFAULT 0 CHECK (is_delete IN (0,1));
ALTER TABLE order_details ADD COLUMN is_delete SMALLINT DEFAULT 0 CHECK (is_delete IN (0,1));

-- 更新视图（添加is_delete条件）
CREATE OR REPLACE VIEW v_order_details AS
SELECT
    od.order_id,
    u.username,
    od.quantity * od.unit_price AS amount,
    od.created_at,
    od.updated_at
FROM order_details od
         JOIN orders o ON od.order_id = o.id AND o.is_delete = 0
         JOIN users u ON o.user_id = u.id AND u.is_delete = 0
WHERE od.is_delete = 0;