#include "radio_stations.h"

static const radio_station_t s_stations[] = {
    {
        .name = "动感101",
        .region = "上海",
        .url = "https://lhttp.qtfm.cn/live/274/64k.mp3",
    },
    {
        .name = "Love Radio",
        .region = "上海",
        .url = "https://lhttp.qtfm.cn/live/273/64k.mp3",
    },
    {
        .name = "经典947",
        .region = "上海",
        .url = "https://lhttp.qtfm.cn/live/267/64k.mp3",
    },
    {
        .name = "交通广播",
        .region = "上海",
        .url = "https://lhttp.qtfm.cn/live/266/64k.mp3",
    },
    {
        .name = "新闻广播",
        .region = "上海",
        .url = "https://lhttp.qtfm.cn/live/270/64k.mp3",
    },
    {
        .name = "中国之声",
        .region = "中央",
        .url = "https://lhttp.qtfm.cn/live/15318317/64k.mp3",
    },
    {
        .name = "山东经济广播",
        .region = "山东",
        .url = "https://lhttp.qtfm.cn/live/20236/64k.mp3",
    },
    {
        .name = "山东文艺广播",
        .region = "山东",
        .url = "https://lhttp.qtfm.cn/live/20238/64k.mp3",
    },
    {
        .name = "山东音乐广播",
        .region = "山东",
        .url = "https://lhttp.qtfm.cn/live/1665/64k.mp3",
    },
    {
        .name = "济南新闻广播",
        .region = "济南",
        .url = "https://lhttp.qtfm.cn/live/1667/64k.mp3",
    },
    {
        .name = "济南交通广播",
        .region = "济南",
        .url = "https://lhttp.qtfm.cn/live/1669/64k.mp3",
    },
    {
        .name = "济南音乐广播",
        .region = "济南",
        .url = "https://lhttp.qtfm.cn/live/1671/64k.mp3",
    },
    {
        .name = "济南故事广播",
        .region = "济南",
        .url = "https://lhttp.qtfm.cn/live/1672/64k.mp3",
    },
    {
        .name = "江苏故事广播",
        .region = "江苏",
        .url = "https://lhttp.qtfm.cn/live/20012/64k.mp3",
    },
    {
        .name = "北京新闻广播",
        .region = "北京",
        .url = "https://lhttp.qtfm.cn/live/339/64k.mp3",
    },
    {
        .name = "AsiaFM亚洲天空台",
        .region = "亚洲",
        .url = "https://lhttp.qingting.fm/live/20071/64k.mp3",
    },
    {
        .name = "两广之声音乐台",
        .region = "两广",
        .url = "https://lhttp.qtfm.cn/live/20500149/64k.mp3",
    },
    {
        .name = "香港电台第一台",
        .region = "香港",
        .url = "https://stm.rthk.hk/radio1",
    },
    {
        .name = "香港电台第二台",
        .region = "香港",
        .url = "https://stm.rthk.hk/radio2",
    },
    {
        .name = "香港电台第三台",
        .region = "香港",
        .url = "https://stm.rthk.hk/radio3",
    },
    {
        .name = "香港电台普通话台",
        .region = "香港",
        .url = "https://stm.rthk.hk/radiopth",
    },
    {
        .name = "BBC World Service",
        .region = "英国",
        .url = "https://stream.live.vc.bbcmedia.co.uk/bbc_world_service",
    },
    {
        .name = "CNN International",
        .region = "美国",
        .url = "https://tunein.cdnstream1.com/2868_96.mp3",
    },
    {
        .name = "NPR Program Stream",
        .region = "美国",
        .url = "https://npr-ice.streamguys1.com/live.mp3",
    },
};

_Static_assert(sizeof(s_stations) / sizeof(s_stations[0]) ==
                   RADIO_STATION_COUNT,
               "RADIO_STATION_COUNT must match s_stations");

size_t radio_station_count(void) {
    return RADIO_STATION_COUNT;
}

const radio_station_t *radio_station_get(size_t index) {
    return index < radio_station_count() ? &s_stations[index] : NULL;
}
