#pragma once

namespace errors{
    inline constexpr char VALIDATION_ERROR[] = "VALIDATION_ERROR";              // 400 参数缺失 格式错误或数值越界
    inline constexpr char AUTH_REQUIRED[] = "AUTH_REQUIRED";                    // 401 未登录或登录会话失效
    inline constexpr char FORBIDDEN[] = "FORBIDDEN";                            // 403 当前用户不允许执行该操作
    inline constexpr char CSRF_REJECTED[] = "CSRF_REJECTED";                    // 403 写请求来源校验失败
    inline constexpr char NOT_FOUND[] = "NOT_FOUND";                            // 404 目标不存在或不属于当前用户
    inline constexpr char NAME_CONFLICT[] = "NAME_CONFLICT";                    // 409 名称已被占用 例如用户名或同目录同类型条目重名
    inline constexpr char VERIFICATION_INVALID[] = "VERIFICATION_INVALID";      // 400 验证码不匹配或尝试次数已用尽
    inline constexpr char VERIFICATION_EXPIRED[] = "VERIFICATION_EXPIRED";      // 400 验证码不存在或已过期
    inline constexpr char PAYLOAD_TOO_LARGE[] = "PAYLOAD_TOO_LARGE";            // 413 上传内容超过对应接口的大小限制
    inline constexpr char UNSUPPORTED_MEDIA_TYPE[] = "UNSUPPORTED_MEDIA_TYPE";  // 415 请求体或上传内容类型不受该接口支持
    inline constexpr char RATE_LIMITED[] = "RATE_LIMITED";                      // 429 发送冷却或请求频率超过限制
    inline constexpr char SERVICE_UNAVAILABLE[] = "SERVICE_UNAVAILABLE";        // 503 数据库 Redis 邮件或存储服务暂时不可用
    inline constexpr char INTERNAL_ERROR[] = "INTERNAL_ERROR";                  // 500 未预期的内部错误
}