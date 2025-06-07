#include "OrderDispatcher.h"
static std::atomic<int> current_page(1);
constexpr int PAGE_SIZE = 100;
int main(){
    SociDB::init();
    RedisUtils::Initialize();
    auto pageResult = fetchPendingOrders<Order>(current_page,PAGE_SIZE);
    if (!pageResult.items.empty()) {
        publish_orders_with_stats(pageResult.items);
        if(current_page>=pageResult.total_pages){
            current_page.store(1); // 重置为1而非0
        }else{
            current_page++;
        }
    }
}
