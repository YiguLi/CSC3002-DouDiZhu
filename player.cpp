#include "player.h"
#include "game.h"
#include <algorithm>
#include <iostream>

Player::Player(Game& game, int id, PlayerType type)
    : game(game), playerId(id), playerType(type), score(0) {}

void Player::NewGame() {
    cards.clear();
    selection.Clear();
    discard.Clear();
    analyse.clear();
    passed = false;
}

void Player::AddCard(int num) {
    cards.insert(num);
}

int Player::GetRemain() const {
    return cards.size();
}

std::string Player::GetName() const {
    if (playerType == LocalPlayer) return "本地玩家";
    if (playerType == NetworkPlayer) return "网络玩家" + std::to_string(playerId);
    return "AI玩家" + std::to_string(playerId);
}

// 叫地主逻辑
int Player::CallLandlord(int questioned, int maxScore) {
    if (playerType == LocalPlayer) {
        // 本地玩家,由外部输入决定
        return -1;  // 需要外部输入
    }
    
    if (playerType == NetworkPlayer) {
        // 网络玩家,等待网络消息
        return -1;
    }

    // AI简单逻辑: 根据手牌质量决定
    int goodCards = 0;
    for (int card : cards) {
        int val = CardGroup::Translate(card);
        if (val >= 14) goodCards++;  // A, 2, 王
    }

    // 根据好牌数量和当前最高分决定
    if (goodCards >= 6 && maxScore < 3) return 3;
    if (goodCards >= 4 && maxScore < 2) return 2;
    if (goodCards >= 2 && maxScore < 1) return 1;

    return 0;  // 不叫
}

void Player::SelectCards(const std::vector<int>& indices) {
    selection.Clear();

    // IMPORTANT: DisplayCards() shows indices after sorting by translated value.
    // We must mirror that ordering here so the user's chosen indices map correctly.
    std::vector<int> cardList(cards.begin(), cards.end());
    std::sort(cardList.begin(), cardList.end(), [](int a, int b){
        return CardGroup::Translate(a) < CardGroup::Translate(b);
    });
    for (int idx : indices) {
        if (idx >= 0 && idx < (int)cardList.size()) {
            selection.AddNumber(cardList[idx]);
        }
    }
    OnSelectionChanged();
}

void Player::ClearSelection() {
    selection.Clear();
    OnSelectionChanged();
}

// 分析选中的牌型
void Player::AnalyseSelection() {
    if (selection.count == 0) {
        selection.type = Unknown;
        return;
    }

    auto& group = selection.group;

    // 单张
    if (selection.count == 1) {
        selection.type = Single;
        selection.value = group.begin()->first;
        return;
    }

    // 对子
    if (selection.count == 2) {
        if (group.size() == 1 && group.begin()->second == 2) {
            selection.type = Double;
            selection.value = group.begin()->first;
            return;
        }
        // 王炸
        if (group.size() == 2 && group.count(16) && group.count(17)) {
            selection.type = KingBomb;
            selection.value = 17;
            return;
        }
    }

    // 三张
    if (selection.count == 3 && group.size() == 1 && group.begin()->second == 3) {
        selection.type = Three;
        selection.value = group.begin()->first;
        return;
    }

    // 炸弹
    if (selection.count == 4 && group.size() == 1 && group.begin()->second == 4) {
        selection.type = Bomb;
        selection.value = group.begin()->first;
        return;
    }

    // 三带一
    if (selection.count == 4 && group.size() == 2) {
        for (auto& p : group) {
            if (p.second == 3) {
                selection.type = ThreePlusOne;
                selection.value = p.first;
                return;
            }
        }
    }

    // 三带二
    if (selection.count == 5 && group.size() == 2) {
        for (auto& p : group) {
            if (p.second == 3) {
                selection.type = ThreePlusTwo;
                selection.value = p.first;
                return;
            }
        }
    }

    // 顺子(至少5张)
    if (selection.count >= 5 && group.size() == selection.count) {
        std::vector<int> vals;
        for (auto& p : group) {
            if (p.first >= 15) {  // 2和王不能在顺子中
                selection.type = Unknown;
                return;
            }
            vals.push_back(p.first);
        }
        std::sort(vals.begin(), vals.end());
        bool isStraight = true;
        for (size_t i = 1; i < vals.size(); ++i) {
            if (vals[i] != vals[i-1] + 1) {
                isStraight = false;
                break;
            }
        }
        if (isStraight) {
            selection.type = SingleSeq;
            selection.value = vals[0];
            return;
        }
    }

    // 连对(至少3对)
    if (selection.count >= 6 && selection.count % 2 == 0 &&
        group.size() == selection.count / 2) {
        std::vector<int> vals;
        for (auto& p : group) {
            if (p.second != 2 || p.first >= 15) {
                selection.type = Unknown;
                return;
            }
            vals.push_back(p.first);
        }
        std::sort(vals.begin(), vals.end());
        bool isStraight = true;
        for (size_t i = 1; i < vals.size(); ++i) {
            if (vals[i] != vals[i-1] + 1) {
                isStraight = false;
                break;
            }
        }
        if (isStraight) {
            selection.type = DoubleSeq;
            selection.value = vals[0];
            return;
        }
    }

    // 四带二（两张单）: 6张，(4,1,1)
    if (selection.count == 6 && group.size() == 3) {
        int fourVal = -1; int singles = 0;
        for (auto &p : group) {
            if (p.second == 4) fourVal = p.first;
            else if (p.second == 1) singles++;
        }
        if (fourVal != -1 && singles == 2) {
            selection.type = FourPlusTwoSingle;
            selection.value = fourVal;
            return;
        }
    }

    // 四带二（两个对子）: 8张，(4,2,2)
    if (selection.count == 8 && group.size() == 3) {
        int fourVal = -1; int pairCnt = 0; bool invalid = false;
        for (auto &p : group) {
            if (p.second == 4) fourVal = p.first;
            else if (p.second == 2) pairCnt++;
            else invalid = true;
        }
        if (!invalid && fourVal != -1 && pairCnt == 2) {
            selection.type = FourPlusTwoPair;
            selection.value = fourVal;
            return;
        }
    }

    // 飞机不带翅膀（纯三顺）: 至少两连（三张起飞）
    if (selection.count % 3 == 0 && selection.count >= 6) {
        std::vector<int> tripleVals;
        for (auto &p : group) if (p.second == 3 && p.first < 15) tripleVals.push_back(p.first);
        std::sort(tripleVals.begin(), tripleVals.end());
        if (!tripleVals.empty()) {
            bool consecutive = true;
            for (size_t i = 1; i < tripleVals.size(); ++i) {
                if (tripleVals[i] != tripleVals[i-1] + 1) { consecutive = false; break; }
            }
            if (consecutive && (int)tripleVals.size()*3 == selection.count) {
                selection.type = ThreeSeq;
                selection.value = tripleVals.front();
                return;
            }
        }
    }

    // 飞机带单：总数为 4n，包含 n 个连续三张 + n 张单牌
    if (selection.count >= 8 && selection.count % 4 == 0) {
        std::vector<int> tripleVals;
        int singles = 0; bool ok = true;
        for (auto &p : group) {
            if (p.second == 3 && p.first < 15) tripleVals.push_back(p.first);
            else if (p.second == 1) singles++;
            else { ok = false; break; }
        }
        std::sort(tripleVals.begin(), tripleVals.end());
        bool consecutive = !tripleVals.empty();
        for (size_t i = 1; i < tripleVals.size(); ++i) {
            if (tripleVals[i] != tripleVals[i-1] + 1) { consecutive = false; break; }
        }
        int n = (int)tripleVals.size();
        if (ok && consecutive && n >= 2 && singles == n && 4*n == selection.count) {
            selection.type = AirplaneSingles;
            selection.value = tripleVals.front();
            return;
        }
    }

    // 飞机带双：总数为 5n，包含 n 个连续三张 + n 个对子
    if (selection.count >= 10 && selection.count % 5 == 0) {
        std::vector<int> tripleVals;
        int pairCnt = 0; bool ok = true;
        for (auto &p : group) {
            if (p.second == 3 && p.first < 15) tripleVals.push_back(p.first);
            else if (p.second == 2) pairCnt++;
            else { ok = false; break; }
        }
        std::sort(tripleVals.begin(), tripleVals.end());
        bool consecutive = !tripleVals.empty();
        for (size_t i = 1; i < tripleVals.size(); ++i) {
            if (tripleVals[i] != tripleVals[i-1] + 1) { consecutive = false; break; }
        }
        int n = (int)tripleVals.size();
        if (ok && consecutive && n >= 2 && pairCnt == n && 5*n == selection.count) {
            selection.type = AirplanePairs;
            selection.value = tripleVals.front();
            return;
        }
    }

    selection.type = Unknown;
}

bool Player::IsValid() {
    AnalyseSelection();
    return selection.type != Unknown;
}

bool Player::CanBeat(const CardGroup& target) {
    if (selection.count == 0) return false;

    AnalyseSelection();

    // 王炸最大
    if (selection.type == KingBomb) return true;

    // 炸弹可以压任何非炸弹牌
    if (selection.type == Bomb) {
        if (target.type != Bomb && target.type != KingBomb) return true;
        if (target.type == Bomb) return selection.value > target.value;
        return false;
    }

    // 其他牌型必须同类型且权值更大
    if (selection.type != target.type) return false;
    if (selection.count != target.count) return false;

    return selection.value > target.value;
}

// AI出牌逻辑
void Player::AISelectCards() {
    selection.Clear();
    const CardGroup& lastDiscard = game.GetLastDiscard();
    CardGroup target = lastDiscard;
    if (game.GetLastPlayer() == this || lastDiscard.count == 0) {
        target.Clear();
    }

    auto options = FindPlayableCards(target);
    if (options.empty()) {
        // 没有可出的牌
        selection.Clear();
        return;
    }
    // 简单AI策略：选择权值最小的可行方案
    // 已经在 FindPlayableCards 中按从小到大生成，取第一个
    selection = options.front();
}

std::vector<CardGroup> Player::FindPlayableCards(const CardGroup& target) const {
    std::vector<CardGroup> res;
    // 构建频次表（基于 Translate 统一转换）
    std::map<int,int> freq;
    for (int c : cards) {
        int v = CardGroup::Translate(c);
        freq[v]++;
    }

    auto push_single = [&](int minGreater){
        for (int c : cards) {
            int v = CardGroup::Translate(c);
            if (v > minGreater) {
                CardGroup g; g.AddNumber(c); g.type = Single; g.value = v; return g; // earliest small
            }
        }
        return CardGroup();
    };

    // 自由出牌：给出最小单张
    if (target.count == 0) {
        if (!cards.empty()) {
            int c = *cards.begin();
            CardGroup g; g.AddNumber(c); g.type = Single; g.value = CardGroup::Translate(c);
            res.push_back(g);
        }
        return res;
    }

    // 针对不同类型，寻找同类型更大者，若无则尝试炸弹/王炸
    auto try_bombs = [&](){
        for (auto &p : freq) if (p.second == 4) {
                // 取该四张的具体牌
                CardGroup g; g.type = Bomb; g.value = p.first;
                for (int c : cards) if (CardGroup::Translate(c) == p.first) g.AddNumber(c);
                res.push_back(g);
            }
        // 王炸
        if (freq.count(16) && freq.count(17)) {
            CardGroup g; g.type = KingBomb; g.value = 17;
            for (int c : cards) { int v = CardGroup::Translate(c); if (v==16 || v==17) g.AddNumber(c); }
            res.push_back(g);
        }
    };

    switch (target.type) {
    case Single: {
        // 找到比 target.value 大的最小单张
        std::vector<std::pair<int,int>> sorted; // (value, card)
        for (int c : cards) sorted.emplace_back(CardGroup::Translate(c), c);
        std::sort(sorted.begin(), sorted.end());
        for (auto &vc : sorted) if (vc.first > target.value) { CardGroup g; g.AddNumber(vc.second); g.type=Single; g.value=vc.first; res.push_back(g); }
        break;
    }
    case Double: {
        for (auto &p : freq) if (p.second >= 2 && p.first > target.value && p.first < 18) {
                CardGroup g; g.type = Double; g.value = p.first; int added=0;
                for (int c : cards) if (CardGroup::Translate(c)==p.first && added<2) { g.AddNumber(c); added++; }
                if (g.GetCount()==2) res.push_back(g);
            }
        break;
    }
    case Three: {
        for (auto &p : freq) if (p.second >= 3 && p.first > target.value && p.first < 18) {
                CardGroup g; g.type = Three; g.value = p.first; int added=0;
                for (int c : cards) if (CardGroup::Translate(c)==p.first && added<3) { g.AddNumber(c); added++; }
                if (g.GetCount()==3) res.push_back(g);
            }
        break;
    }
    case ThreePlusOne: {
        for (auto &p : freq) if (p.second >= 3 && p.first > target.value) {
                for (auto &s : freq) if (s.first != p.first && s.second >= 1) {
                        CardGroup g; g.type = ThreePlusOne; g.value = p.first; int a=0;
                        for (int c : cards) if (CardGroup::Translate(c)==p.first && a<3) { g.AddNumber(c); a++; }
                        // one single
                        for (int c : cards) if (CardGroup::Translate(c)==s.first) { g.AddNumber(c); break; }
                        if (g.GetCount()==4) res.push_back(g);
                    }
            }
        break;
    }
    case ThreePlusTwo: {
        for (auto &p : freq) if (p.second >= 3 && p.first > target.value) {
                for (auto &s : freq) if (s.first != p.first && s.second >= 2) {
                        CardGroup g; g.type = ThreePlusTwo; g.value = p.first; int a=0;
                        for (int c : cards) if (CardGroup::Translate(c)==p.first && a<3) { g.AddNumber(c); a++; }
                        int b=0; for (int c : cards) if (CardGroup::Translate(c)==s.first && b<2) { g.AddNumber(c); b++; }
                        if (g.GetCount()==5) res.push_back(g);
                    }
            }
        break;
    }
    case SingleSeq: {
        int len = target.count;
        // 构造所有可能起点（3..14），不含2和王
        for (int start=3; start<=14 && start+len-1<=14; ++start) {
            bool ok=true; for (int v=start; v<start+len; ++v) if (!freq.count(v) || freq[v] < 1) { ok=false; break; }
            if (ok && start > target.value) {
                CardGroup g; g.type = SingleSeq; g.value = start; for (int v=start; v<start+len; ++v) {
                    // 拿出每个值的一张
                    for (int c : cards) if (CardGroup::Translate(c)==v) { g.AddNumber(c); break; }
                }
                res.push_back(g);
            }
        }
        break;
    }
    case DoubleSeq: {
        int pairs = target.count/2;
        for (int start=3; start<=14 && start+pairs-1<=14; ++start) {
            bool ok=true; for (int v=start; v<start+pairs; ++v) if (freq[v] < 2) { ok=false; break; }
            if (ok && start > target.value) {
                CardGroup g; g.type = DoubleSeq; g.value = start; for (int v=start; v<start+pairs; ++v) {
                    int added=0; for (int c : cards) if (CardGroup::Translate(c)==v && added<2) { g.AddNumber(c); added++; }
                }
                res.push_back(g);
            }
        }
        break;
    }
    case FourPlusTwoSingle: {
        for (auto &p : freq) if (p.second >= 4 && p.first > target.value) {
                // 选择任意两张单
                std::vector<int> singlesCards;
                for (auto &s : freq) if (s.first != p.first && s.second >= 1) {
                        for (int c : cards) if (CardGroup::Translate(c)==s.first) singlesCards.push_back(c);
                    }
                if (singlesCards.size() >= 2) {
                    CardGroup g; g.type = FourPlusTwoSingle; g.value = p.first; int a=0;
                    for (int c : cards) if (CardGroup::Translate(c)==p.first && a<4) { g.AddNumber(c); a++; }
                    g.AddNumber(singlesCards[0]); g.AddNumber(singlesCards[1]);
                    res.push_back(g);
                }
            }
        break;
    }
    case FourPlusTwoPair: {
        for (auto &p : freq) if (p.second >= 4 && p.first > target.value) {
                std::vector<int> pairVals; for (auto &s : freq) if (s.first != p.first && s.second >= 2) pairVals.push_back(s.first);
                for (size_t i=0; i<pairVals.size(); ++i) for (size_t j=i+1; j<pairVals.size(); ++j) {
                        CardGroup g; g.type = FourPlusTwoPair; g.value = p.first; int a=0;
                        for (int c : cards) if (CardGroup::Translate(c)==p.first && a<4) { g.AddNumber(c); a++; }
                        int b=0; for (int c : cards) if (CardGroup::Translate(c)==pairVals[i] && b<2) { g.AddNumber(c); b++; }
                        int d=0; for (int c : cards) if (CardGroup::Translate(c)==pairVals[j] && d<2) { g.AddNumber(c); d++; }
                        if (g.GetCount()==8) res.push_back(g);
                    }
            }
        break;
    }
    case ThreeSeq: {
        int groups = target.count/3;
        for (int start=3; start<=14 && start+groups-1<=14; ++start) {
            bool ok=true; for (int v=start; v<start+groups; ++v) if (freq[v] < 3) { ok=false; break; }
            if (ok && start > target.value) {
                CardGroup g; g.type = ThreeSeq; g.value = start; for (int v=start; v<start+groups; ++v) {
                    int a=0; for (int c : cards) if (CardGroup::Translate(c)==v && a<3) { g.AddNumber(c); a++; }
                }
                res.push_back(g);
            }
        }
        break;
    }
    case AirplaneSingles: {
        int n = target.count/4; // triples count
        for (int start=3; start<=14 && start+n-1<=14; ++start) {
            bool ok=true; for (int v=start; v<start+n; ++v) if (freq[v] < 3) { ok=false; break; }
            if (!ok || start <= target.value) continue;
            // collect n singles excluding triple values where possible
            std::vector<int> singlesCards;
            for (auto &p : freq) if (p.first < 18 && (p.first < start || p.first >= start+n)) {
                    for (int c : cards) if (CardGroup::Translate(c)==p.first) singlesCards.push_back(c);
                }
            if ((int)singlesCards.size() < n) continue;
            CardGroup g; g.type = AirplaneSingles; g.value = start;
            for (int v=start; v<start+n; ++v) { int a=0; for (int c : cards) if (CardGroup::Translate(c)==v && a<3) { g.AddNumber(c); a++; } }
            for (int i=0;i<n;++i) g.AddNumber(singlesCards[i]);
            if (g.GetCount()==4*n) res.push_back(g);
        }
        break;
    }
    case AirplanePairs: {
        int n = target.count/5; // triples count
        for (int start=3; start<=14 && start+n-1<=14; ++start) {
            bool ok=true; for (int v=start; v<start+n; ++v) if (freq[v] < 3) { ok=false; break; }
            if (!ok || start <= target.value) continue;
            // collect n pairs excluding triple values where possible
            std::vector<int> pairVals; for (auto &p : freq) if ((p.first < start || p.first >= start+n) && p.second >= 2) pairVals.push_back(p.first);
            if ((int)pairVals.size() < n) continue;
            CardGroup g; g.type = AirplanePairs; g.value = start;
            for (int v=start; v<start+n; ++v) { int a=0; for (int c : cards) if (CardGroup::Translate(c)==v && a<3) { g.AddNumber(c); a++; } }
            for (int i=0;i<n;++i) { int b=0; for (int c : cards) if (CardGroup::Translate(c)==pairVals[i] && b<2) { g.AddNumber(c); b++; } }
            if (g.GetCount()==5*n) res.push_back(g);
        }
        break;
    }
    case Bomb: {
        for (auto &p : freq) if (p.second == 4 && p.first > target.value) {
                CardGroup g; g.type = Bomb; g.value = p.first; int a=0; for (int c : cards) if (CardGroup::Translate(c)==p.first && a<4) { g.AddNumber(c); a++; }
                res.push_back(g);
            }
        // 王炸也可压
        if (freq.count(16) && freq.count(17)) {
            CardGroup g; g.type = KingBomb; g.value = 17; for (int c : cards) { int v=CardGroup::Translate(c); if (v==16||v==17) g.AddNumber(c);} res.push_back(g);
        }
        break;
    }
    case KingBomb: {
        // 无法被压，返回空
        break;
    }
    default: break;
    }

    // 如果没有找到同类型可压，考虑炸弹与王炸
    if (!res.empty()) {
        // 排序：按 type 次序和 value 升序，这里简单按 value 升序
        std::sort(res.begin(), res.end(), [](const CardGroup& a, const CardGroup& b){ return a.value < b.value; });
        return res;
    }
    try_bombs();
    std::sort(res.begin(), res.end(), [](const CardGroup& a, const CardGroup& b){ return a.value < b.value; });
    return res;
}

bool Player::Discard() {
    if (playerType == LocalPlayer) {
        bool ok = HumanDiscard();
        if (ok) { passed = false; }
        return ok;
    }
    
    if (playerType == NetworkPlayer) {
        // 网络玩家,等待网络消息,不在此处出牌
        return false;
    }

    // AI玩家自动出牌
    AISelectCards();

    if (selection.count == 0) {
        Pass();
        return false;
    }

    // 执行出牌
    for (int card : selection.cards) {
        auto it = cards.find(card);
        if (it != cards.end()) cards.erase(it);
    }

    discard = selection;
    selection.Clear();
    passed = false;
    return true;
}

bool Player::HumanDiscard() {
    if (selection.count == 0) return false;

    if (!IsValid()) {
        std::cout << "选择的牌型无效!" << std::endl;
        return false;
    }

    const CardGroup& lastDiscard = game.GetLastDiscard();
    if (game.GetLastPlayer() != this && lastDiscard.count > 0) {
        if (!CanBeat(lastDiscard)) {
            std::cout << "无法压过上家的牌!" << std::endl;
            return false;
        }
    }

    // 执行出牌
    for (int card : selection.cards) {
        auto it = cards.find(card);
        if (it != cards.end()) cards.erase(it);
    }

    discard = selection;
    selection.Clear();
    passed = false;
    return true;
}

void Player::Pass() {
    selection.Clear();
    passed = true;
}

std::vector<CardGroup> Player::Hint() {
    // 简单提示: 找一张能压过的牌
    const CardGroup& lastDiscard = game.GetLastDiscard();
    selection.Clear();

    CardGroup target = lastDiscard;
    if (game.GetLastPlayer() == this || lastDiscard.count == 0) {
        target.Clear();
    }

    auto options = FindPlayableCards(target);
    if (!options.empty()) {
        // 预选第一个方案到 selection，便于 UI 直接出
        selection = options.front();
    }
    return options;
}

void Player::OnSelectionChanged() {
    // 选牌改变时立即分析，形成"缓存"
    AnalyseSelection();
}

// 获取手牌向量（用于网络同步）
std::vector<int> Player::GetHandCards() const {
    std::vector<int> handCards;
    for (int card : cards) {
        handCards.push_back(card);
    }
    return handCards;
}

// 设置手牌（用于网络同步）
void Player::SetHandCards(const std::vector<int>& handCards) {
    cards.clear();
    for (int card : handCards) {
        cards.insert(card);
    }
    // 手牌已设置，无需特别分析（按需分析即可）
}

