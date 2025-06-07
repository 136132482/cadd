#include <iostream>
#include <string>
#include <optional>
#include <regex>
#include <cmath>
#include <cctype>

class GeometryConverter {
public:
    struct Point {
        std::string x_str;
        std::string y_str;

        bool isValid() const {
            try {
                double x = std::stod(x_str);
                double y = std::stod(y_str);
                return !std::isnan(x) && !std::isnan(y) &&
                       x >= -180.0 && x <= 180.0 &&
                       y >= -90.0 && y <= 90.0;
            } catch (...) {
                return false;
            }
        }
    };

    // 新增：判断字符串是否是合法的POINT格式
    static bool isPointString(const std::string& input) {
        auto trim = [](std::string s) { // 注意这里改为值传递，不修改原字符串
            s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) { return !std::isspace(ch); }));
            s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) { return !std::isspace(ch); }).base(), s.end());
            return s;
        };

        std::string s = trim(input);

        // 检查是否以POINT开头（不区分大小写）
        if (s.size() < 6 ||
            std::tolower(s[0]) != 'p' ||
            std::tolower(s[1]) != 'o' ||
            std::tolower(s[2]) != 'i' ||
            std::tolower(s[3]) != 'n' ||
            std::tolower(s[4]) != 't') {
            return false;
        }

        // 检查括号结构
        size_t left_brace = s.find('(');
        size_t right_brace = s.find(')');

        return left_brace != std::string::npos &&
               right_brace != std::string::npos &&
               left_brace < right_brace;
    }

    // 修改后的解析函数（使用isPointString先做判断）
    static std::optional<Point> stringToPoint(std::string input) {
        auto trim = [](std::string& s) {
            s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) { return !std::isspace(ch); }));
            s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) { return !std::isspace(ch); }).base(), s.end());
        };

        trim(input);

        // 先判断是否是POINT格式
        if (isPointString(input)) {
            size_t left_brace = input.find('(');
            size_t right_brace = input.find(')');

            // 提取坐标部分
            std::string coords = input.substr(left_brace + 1, right_brace - left_brace - 1);
            trim(coords);

            // 分割坐标
            size_t space_pos = coords.find(' ');
            if (space_pos == std::string::npos) space_pos = coords.find('\t');

            if (space_pos != std::string::npos) {
                Point pt{
                        coords.substr(0, space_pos),
                        coords.substr(space_pos + 1)
                };
                trim(pt.x_str);
                trim(pt.y_str);

                if (!pt.x_str.empty() && !pt.y_str.empty()) {
                    return pt.isValid() ? std::optional<Point>(pt) : std::nullopt;
                }
            }
        }

        // 解析纯坐标格式
        size_t separator = input.find_first_of(", \t");
        if (separator != std::string::npos) {
            Point pt{
                    input.substr(0, separator),
                    input.substr(separator + 1)
            };
            trim(pt.x_str);
            trim(pt.y_str);

            if (!pt.x_str.empty() && !pt.y_str.empty()) {
                return pt.isValid() ? std::optional<Point>(pt) : std::nullopt;
            }
        }

        return std::nullopt;
    }

    static std::string pointToString(const Point& pt) {
        if (!pt.isValid()) return "INVALID_POINT";
        return "POINT(" + pt.x_str + " " + pt.y_str + ")";
    }
};

//int main() {
//    auto test = [](const std::string& s) {
//        // 先测试isPointString
//        std::cout << "输入: \"" << s << "\"";
//        if (GeometryConverter::isPointString(s)) {
//            std::cout << " 是POINT格式";
//            // 如果是POINT格式，再测试转换
//            auto pt = GeometryConverter::stringToPoint(s);
//            std::cout << ", 转换结果: " << (pt ? GeometryConverter::pointToString(*pt) : "转换失败");
//        } else {
//            std::cout << " 不是POINT格式";
//        }
//        std::cout << "\n\n";
//    };
//
//    // 测试用例
//    test("POINT(116.404 39.915)");
//    test("POINT ( 116.404 39.915 )");
//    test("point(116.404 39.915)");
//    test("POINT(116.404000 39.9150)");
//    test("POINT(116 39)");
//    test("POINT(-12.3456 +78.9012)");
//    test("116.404,39.915");
//    test("116.404 39.915");
//    test(" 116.404 , 39.915 ");
//    test("invalid");
//    test("This is a POINT example"); // 包含POINT但不是几何格式
//    test("MULTIPOINT(1 1)"); // 其他几何类型
//
//    return 0;
//}