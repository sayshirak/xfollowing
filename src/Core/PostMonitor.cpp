#include "PostMonitor.h"
#include "Utils/LanguageFilter.h"
#include <QJsonArray>
#include <QJsonDocument>

PostMonitor::PostMonitor(QObject* parent)
    : QObject(parent) {
}

QString PostMonitor::buildKeywordsArray(const QList<Keyword>& keywords) {
    QJsonArray arr;
    for (const auto& kw : keywords) {
        if (kw.isEnabled) {
            arr.append(kw.text);
        }
    }
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

QString PostMonitor::getMonitorScript(const QList<Keyword>& keywords,
                                      const QString& selectedLang,
                                      const QList<Keyword>& blacklistKeywords) {
    QString keywordsJson = buildKeywordsArray(keywords);
    QString blacklistJson = buildKeywordsArray(blacklistKeywords);
    QString selectedJson = LanguageFilter::buildSelectedLangJson(selectedLang);
    QString detectJs = LanguageFilter::jsDetectLanguageFunction();

    QString script = QString::fromUtf8(R"JS(
(function() {
    if (window.xfollowingObserver) {
        window.xfollowingObserver.disconnect();
    }
    if (!window.xfollowingProcessedIds) {
        window.xfollowingProcessedIds = new Set();
    }

    const keywords = )JS");
    script += keywordsJson;
    script += QString::fromUtf8(R"JS(;
    const blacklistKeywords = )JS");
    script += blacklistJson;
    script += QString::fromUtf8(R"JS(;
    const selectedLang = )JS");
    script += selectedJson;
    script += QString::fromUtf8(R"JS(;
)JS");
    script += detectJs;
    script += QString::fromUtf8(R"JS(

    function hitsBlacklist(name, handle) {
        const n = (name || '').toLowerCase();
        const h = (handle || '').toLowerCase();
        for (const kw of blacklistKeywords) {
            const t = (kw || '').toLowerCase();
            if (!t) continue;
            if (n.includes(t) || h.includes(t)) return t;
        }
        return null;
    }

    function extractBioFromArticle(article) {
        const desc = article.querySelector('[data-testid="UserDescription"]');
        if (desc && desc.innerText) return desc.innerText.trim();
        const cell = article.closest('[data-testid="UserCell"]') || article.querySelector('[data-testid="UserCell"]');
        if (cell) {
            const spans = cell.querySelectorAll('span');
            for (const span of spans) {
                const t = (span.innerText || '').trim();
                if (t.length >= 4 && !t.startsWith('@') && !t.match(/^\d/) &&
                    t.toLowerCase() !== 'follows you' && t !== '关注了你') {
                    const parentTest = span.closest('[data-testid="UserName"]');
                    if (!parentTest && t.length > 8) return t;
                }
            }
        }
        return '';
    }

    function clipText(s, n) {
        s = String(s || '').replace(/\s+/g, ' ').trim();
        if (s.length <= n) return s;
        return s.slice(0, n) + '…';
    }

    // 显示昵称：优先作者主页链接内文案，再 UserName；绝不把 @id / handle 当昵称
    function isBadDisplayName(t, handle) {
        if (!t) return true;
        t = String(t).replace(/\s+/g, ' ').trim();
        if (!t || t.startsWith('@')) return true;
        const h = String(handle || '').toLowerCase();
        if (!h) return false;
        const low = t.toLowerCase();
        return low === h || low === ('@' + h);
    }

    function pickNameFromNode(node, handle) {
        if (!node) return '';
        const els = node.querySelectorAll('div[dir="auto"], span[dir="auto"], span');
        for (const el of els) {
            const raw = (el.textContent || el.innerText || '').trim();
            if (!raw) continue;
            const first = raw.split('\n')[0].replace(/\s+/g, ' ').trim();
            if (!isBadDisplayName(first, handle)) return first;
        }
        const top = (node.textContent || node.innerText || '').trim();
        const first = top.split('\n')[0].replace(/\s+/g, ' ').trim();
        if (!isBadDisplayName(first, handle)) return first;
        return '';
    }

    function extractDisplayName(root, handle) {
        if (!root || !handle) return '';
        const href = '/' + handle;
        const links = root.querySelectorAll('a[href="' + href + '"], a[href^="' + href + '?"]');
        for (const link of links) {
            const name = pickNameFromNode(link, handle);
            if (name) return name;
        }
        const blocks = root.querySelectorAll('[data-testid="UserName"], [data-testid="User-Name"]');
        for (const block of blocks) {
            const name = pickNameFromNode(block, handle);
            if (name) return name;
        }
        const tweetText = root.querySelector('[data-testid="tweetText"]');
        const autos = root.querySelectorAll('div[dir="auto"]');
        for (const el of autos) {
            if (tweetText && tweetText.contains(el)) continue;
            const first = (el.textContent || el.innerText || '')
                .split('\n')[0].replace(/\s+/g, ' ').trim();
            if (!isBadDisplayName(first, handle)) return first;
        }
        return '';
    }

    // 返回 { post } 或 { reject, handle, name, why }；why 为中文说明，便于逐条排查
    function parsePost(article) {
        try {
            const authorLinks = article.querySelectorAll('a[href^="/"]');
            let authorHandle = '';
            let authorName = '';

            for (const link of authorLinks) {
                const href = link.getAttribute('href');
                if (href && href.match(/^\/[a-zA-Z0-9_]+$/) && !href.includes('/status/')) {
                    authorHandle = href.substring(1);
                    break;
                }
            }
            authorName = extractDisplayName(article, authorHandle);
            if (!authorName) authorName = authorHandle;

            const who = authorHandle ? ('@' + authorHandle) : '(无handle)';
            const namePart = authorName ? (' 昵称="' + clipText(authorName, 40) + '"') : '';

            if (!authorHandle) {
                return {
                    reject: 'no_handle',
                    handle: '',
                    name: '',
                    why: '无法解析作者 handle，跳过'
                };
            }

            const bl = hitsBlacklist(authorName, authorHandle);
            if (bl) {
                return {
                    reject: 'blacklist',
                    handle: authorHandle,
                    name: authorName,
                    why: who + namePart + ' 命中黑名单关键词="' + bl + '"，跳过'
                };
            }

            const verifiedBadge = article.querySelector('[data-testid="icon-verified"]') ||
                                  article.querySelector('svg[aria-label="Verified account"]') ||
                                  article.querySelector('svg[aria-label="已认证帐号"]') ||
                                  article.querySelector('[aria-label="Verified account"]') ||
                                  article.querySelector('[aria-label="已认证帐号"]');
            if (!verifiedBadge) {
                return {
                    reject: 'no_blue_v',
                    handle: authorHandle,
                    name: authorName,
                    why: who + namePart + ' 未识别到蓝V认证图标，跳过'
                };
            }

            const contentDiv = article.querySelector('[data-testid="tweetText"]');
            const content = contentDiv ? contentDiv.innerText : '';
            if (!content) {
                return {
                    reject: 'no_content',
                    handle: authorHandle,
                    name: authorName,
                    why: who + namePart + ' 已是蓝V，但无帖子正文(tweetText为空)，跳过'
                };
            }

            let matchedKeyword = null;
            for (const kw of keywords) {
                if (content.toLowerCase().includes(kw.toLowerCase())) {
                    matchedKeyword = kw;
                    break;
                }
            }
            if (!matchedKeyword) {
                return {
                    reject: 'no_keyword',
                    handle: authorHandle,
                    name: authorName,
                    why: who + namePart + ' 已是蓝V，但正文不含关键词' + JSON.stringify(keywords) +
                         '；正文="' + clipText(content, 80) + '"，跳过'
                };
            }

            const statusLink = article.querySelector('a[href*="/status/"]');
            const postUrl = statusLink ? 'https://x.com' + statusLink.getAttribute('href') : '';
            const postId = postUrl.split('/status/')[1]?.split('?')[0] || '';
            if (!postId) {
                return {
                    reject: 'no_post_id',
                    handle: authorHandle,
                    name: authorName,
                    why: who + namePart + ' 已匹配关键词"' + matchedKeyword +
                         '"，但无法解析 postId，跳过'
                };
            }

            const timeElement = article.querySelector('time');
            const postTime = timeElement ? timeElement.getAttribute('datetime') : '';

            const bio = extractBioFromArticle(article);
            const nameLang = xfollowDetectLanguage(authorName);
            const bioTrim = (bio || '').trim();
            const bioLang = bioTrim ? xfollowDetectLanguage(bioTrim) : 'other';
            const contentLang = xfollowDetectLanguage(content);
            const langOk = xfollowPassesLanguageFilter(authorName, bio, selectedLang);
            if (!langOk) {
                return {
                    reject: 'lang_fail',
                    handle: authorHandle,
                    name: authorName,
                    why: who + namePart +
                         ' 已是蓝V且正文含关键词"' + matchedKeyword +
                         '"，但语言未过(需昵称或简介为' + selectedLang +
                         ')；昵称语言=' + nameLang +
                         ' 简介语言=' + bioLang +
                         (bioTrim ? (' 简介="' + clipText(bioTrim, 40) + '"') : ' 简介=(空,搜索卡片常无简介)') +
                         ' 正文语言=' + contentLang + '(正文不参与语言判定)' +
                         ' 正文="' + clipText(content, 60) + '"，跳过'
                };
            }

            return {
                post: {
                    postId: postId,
                    authorHandle: authorHandle,
                    authorName: authorName,
                    authorUrl: 'https://x.com/' + authorHandle,
                    content: content,
                    postUrl: postUrl,
                    postTime: postTime,
                    matchedKeyword: matchedKeyword,
                    bio: langOk.bio,
                    nameLang: langOk.nameLang,
                    bioLang: langOk.bioLang
                },
                why: who + namePart + ' 通过全部过滤(蓝V+关键词"' + matchedKeyword +
                     '"+语言' + selectedLang + ' 昵称=' + langOk.nameLang +
                     ' 简介=' + langOk.bioLang + ')，将入库待关注'
            };
        } catch (e) {
            console.log('[XFOLLOW] Parse error:', e);
            return { reject: 'parse_error', handle: '', name: '', why: '解析异常: ' + e };
        }
    }

    function bumpReject(stats, reason) {
        stats.reject[reason] = (stats.reject[reason] || 0) + 1;
    }

    function formatReject(reject) {
        return Object.keys(reject).map(k => k + '=' + reject[k]).join(',') || 'none';
    }

    // forceLog=true 时必定打统计；否则约每 10 秒打一次，避免 MutationObserver 刷屏
    function scanPosts(tag, forceLog) {
        const articles = document.querySelectorAll('article[data-testid="tweet"]');
        const newPosts = [];
        const stats = {
            articles: articles.length,
            already: 0,
            examined: 0,
            pass: 0,
            reject: {}
        };

        articles.forEach(article => {
            const statusLink = article.querySelector('a[href*="/status/"]');
            const href = statusLink ? statusLink.getAttribute('href') : '';
            const postId = href ? href.split('/status/')[1]?.split('?')[0] : '';
            if (!postId) {
                bumpReject(stats, 'no_post_id');
                console.log('[XFOLLOW] 搜索跳过: 页面帖子无 status 链接，无法解析 postId');
                return;
            }
            if (window.xfollowingProcessedIds.has(postId)) {
                stats.already++;
                return;
            }
            window.xfollowingProcessedIds.add(postId);
            stats.examined++;
            const result = parsePost(article);
            if (result.post) {
                stats.pass++;
                newPosts.push(result.post);
                console.log('[XFOLLOW] 搜索可关注: ' + (result.why || ('@' + result.post.authorHandle)));
            } else {
                bumpReject(stats, result.reject || 'unknown');
                console.log('[XFOLLOW] 搜索无法关注: [' + (result.reject || 'unknown') + '] ' +
                            (result.why || '未知原因'));
            }
        });

        const now = Date.now();
        if (!window.xfollowingLastScanLogAt) window.xfollowingLastScanLogAt = 0;
        // examined>0 时也打：否则 MutationObserver 先扫过、全被过滤时，后续只见 already、看不到 reject
        const shouldLog = forceLog || stats.pass > 0 || stats.examined > 0 ||
                          (now - window.xfollowingLastScanLogAt > 10000);
        if (shouldLog) {
            window.xfollowingLastScanLogAt = now;
            console.log('[XFOLLOW] ScanPosts(' + (tag || 'tick') + '): articles=' + stats.articles +
                        ' already=' + stats.already +
                        ' examined=' + stats.examined +
                        ' pass=' + stats.pass +
                        ' reject={' + formatReject(stats.reject) + '}' +
                        ' kw=' + JSON.stringify(keywords) +
                        ' lang=' + selectedLang);
        }

        if (newPosts.length > 0) {
            console.log('XFOLLOWING_NEW_POSTS:' + JSON.stringify(newPosts));
        }
    }

    function initialScan() {
        let scanCount = 0;
        const maxScans = 5;
        function doScan() {
            scanCount++;
            console.log('[XFOLLOW] Initial scan attempt ' + scanCount + '/' + maxScans);
            scanPosts('initial-' + scanCount, true);
            if (scanCount < maxScans) setTimeout(doScan, 1500);
        }
        setTimeout(doScan, 2000);
    }

    if (window.xfollowingInterval) clearInterval(window.xfollowingInterval);
    window.xfollowingInterval = setInterval(() => scanPosts('interval', false), 5000);

    window.xfollowingObserver = new MutationObserver(() => { scanPosts('mut', false); });
    window.xfollowingObserver.observe(document.body, { childList: true, subtree: true });

    initialScan();
    console.log('[XFOLLOW] Monitor script injected, keywords:', keywords, 'blacklist:', blacklistKeywords, 'lang:', selectedLang);
})();
)JS");

    return script;
}

QString PostMonitor::getFollowersMonitorScript(const QString& selectedLang,
                                               const QList<Keyword>& blacklistKeywords) {
    QString blacklistJson = buildKeywordsArray(blacklistKeywords);
    QString selectedJson = LanguageFilter::buildSelectedLangJson(selectedLang);
    QString detectJs = LanguageFilter::jsDetectLanguageFunction();

    QString script = QString::fromUtf8(R"JS(
(function() {
    if (!window.xfollowingFollowersProcessedIds) {
        window.xfollowingFollowersProcessedIds = new Set();
    }

    const blacklistKeywords = )JS");
    script += blacklistJson;
    script += QString::fromUtf8(R"JS(;
    const selectedLang = )JS");
    script += selectedJson;
    script += QString::fromUtf8(R"JS(;
)JS");
    script += detectJs;
    script += QString::fromUtf8(R"JS(

    function hitsBlacklist(name, handle) {
        const n = (name || '').toLowerCase();
        const h = (handle || '').toLowerCase();
        for (const kw of blacklistKeywords) {
            const t = (kw || '').toLowerCase();
            if (!t) continue;
            if (n.includes(t) || h.includes(t)) return t;
        }
        return null;
    }

    function extractBioFromUserCell(userCell) {
        const desc = userCell.querySelector('[data-testid="UserDescription"]');
        if (desc && desc.innerText) return desc.innerText.trim();

        let best = '';
        const spans = userCell.querySelectorAll('div[dir="auto"] span, span');
        for (const span of spans) {
            if (span.closest('[data-testid="UserName"]')) continue;
            const t = (span.innerText || '').trim();
            if (!t || t.startsWith('@')) continue;
            const low = t.toLowerCase();
            if (low === 'follows you' || low.includes('follows you') ||
                t === '关注了你' || t.includes('关注了你')) continue;
            if (t.length > best.length && t.length >= 2) best = t;
        }
        return best;
    }

    function isBadDisplayName(t, handle) {
        if (!t) return true;
        t = String(t).replace(/\s+/g, ' ').trim();
        if (!t || t.startsWith('@')) return true;
        const h = String(handle || '').toLowerCase();
        if (!h) return false;
        const low = t.toLowerCase();
        return low === h || low === ('@' + h);
    }

    function pickNameFromNode(node, handle) {
        if (!node) return '';
        const els = node.querySelectorAll('div[dir="auto"], span[dir="auto"], span');
        for (const el of els) {
            const raw = (el.textContent || el.innerText || '').trim();
            if (!raw) continue;
            const first = raw.split('\n')[0].replace(/\s+/g, ' ').trim();
            if (!isBadDisplayName(first, handle)) return first;
        }
        const top = (node.textContent || node.innerText || '').trim();
        const first = top.split('\n')[0].replace(/\s+/g, ' ').trim();
        if (!isBadDisplayName(first, handle)) return first;
        return '';
    }

    function extractDisplayName(root, handle) {
        if (!root || !handle) return '';
        const href = '/' + handle;
        const links = root.querySelectorAll('a[href="' + href + '"], a[href^="' + href + '?"]');
        for (const link of links) {
            const name = pickNameFromNode(link, handle);
            if (name) return name;
        }
        const blocks = root.querySelectorAll('[data-testid="UserName"], [data-testid="User-Name"]');
        for (const block of blocks) {
            const name = pickNameFromNode(block, handle);
            if (name) return name;
        }
        const autos = root.querySelectorAll('div[dir="auto"]');
        for (const el of autos) {
            if (el.closest('[data-testid="UserDescription"]')) continue;
            const first = (el.textContent || el.innerText || '')
                .split('\n')[0].replace(/\s+/g, ' ').trim();
            if (!isBadDisplayName(first, handle)) return first;
        }
        return '';
    }

    // 返回 { follower } 或 { reject: 原因 }
    function parseFollower(userCell) {
        try {
            const userLinks = userCell.querySelectorAll('a[href^="/"]');
            let userHandle = '';
            let userName = '';

            for (const link of userLinks) {
                const href = link.getAttribute('href');
                if (href && href.match(/^\/[a-zA-Z0-9_]+$/) && !href.includes('/status/')) {
                    userHandle = href.substring(1);
                    break;
                }
            }
            userName = extractDisplayName(userCell, userHandle);
            if (!userName) userName = userHandle;

            if (!userHandle) return { reject: 'no_handle' };
            if (hitsBlacklist(userName, userHandle)) return { reject: 'blacklist' };

            const verifiedBadge = userCell.querySelector('[data-testid="icon-verified"]') ||
                                  userCell.querySelector('svg[aria-label="Verified account"]') ||
                                  userCell.querySelector('svg[aria-label="已认证帐号"]') ||
                                  userCell.querySelector('[aria-label="Verified account"]') ||
                                  userCell.querySelector('[aria-label="已认证帐号"]');
            if (!verifiedBadge) return { reject: 'no_blue_v' };

            const allSpans = userCell.querySelectorAll('span');
            for (const span of allSpans) {
                const text = span.innerText.toLowerCase().trim();
                if (text === 'follows you' || text.includes('follows you') ||
                    text === '关注了你' || text.includes('关注了你')) {
                    return { reject: 'already_follows_you' };
                }
            }

            const bio = extractBioFromUserCell(userCell);
            const langOk = xfollowPassesLanguageFilter(userName, bio, selectedLang);
            if (!langOk) return { reject: 'lang_fail' };

            return {
                follower: {
                    authorHandle: userHandle,
                    authorName: userName,
                    authorUrl: 'https://x.com/' + userHandle,
                    bio: langOk.bio,
                    nameLang: langOk.nameLang,
                    bioLang: langOk.bioLang
                }
            };
        } catch (e) {
            console.log('[XFOLLOW] Parse follower error:', e);
            return { reject: 'parse_error' };
        }
    }

    function bumpReject(stats, reason) {
        stats.reject[reason] = (stats.reject[reason] || 0) + 1;
    }

    function formatReject(reject) {
        return Object.keys(reject).map(k => k + '=' + reject[k]).join(',') || 'none';
    }

    function scanFollowers(forceLog) {
        const userCells = document.querySelectorAll('[data-testid="UserCell"]');
        const newFollowers = [];
        const stats = {
            cells: userCells.length,
            already: 0,
            examined: 0,
            pass: 0,
            reject: {}
        };

        userCells.forEach(cell => {
            const userLinks = cell.querySelectorAll('a[href^="/"]');
            let userHandle = '';
            for (const link of userLinks) {
                const href = link.getAttribute('href');
                if (href && href.match(/^\/[a-zA-Z0-9_]+$/) && !href.includes('/status/')) {
                    userHandle = href.substring(1);
                    break;
                }
            }

            if (!userHandle) {
                bumpReject(stats, 'no_handle');
                return;
            }
            if (window.xfollowingFollowersProcessedIds.has(userHandle)) {
                stats.already++;
                return;
            }
            window.xfollowingFollowersProcessedIds.add(userHandle);
            stats.examined++;
            const result = parseFollower(cell);
            if (result.follower) {
                stats.pass++;
                newFollowers.push(result.follower);
            } else {
                bumpReject(stats, result.reject || 'unknown');
            }
        });

        const now = Date.now();
        if (!window.xfollowingLastFollowerLogAt) window.xfollowingLastFollowerLogAt = 0;
        const shouldLog = forceLog || stats.pass > 0 || stats.examined > 0 ||
                          (now - window.xfollowingLastFollowerLogAt > 10000);
        if (shouldLog) {
            window.xfollowingLastFollowerLogAt = now;
            console.log('[XFOLLOW] ScanFollowers: cells=' + stats.cells +
                        ' already=' + stats.already +
                        ' examined=' + stats.examined +
                        ' pass=' + stats.pass +
                        ' reject={' + formatReject(stats.reject) + '}' +
                        ' lang=' + selectedLang +
                        ' path=' + (window.location.pathname || ''));
        }

        if (newFollowers.length > 0) {
            console.log('XFOLLOWING_NEW_FOLLOWERS:' + JSON.stringify(newFollowers));
        }
    }

    if (window.xfollowingFollowersInterval) clearInterval(window.xfollowingFollowersInterval);
    window.xfollowingFollowersInterval = setInterval(() => scanFollowers(false), 3000);
    setTimeout(() => scanFollowers(true), 2000);
    console.log('[XFOLLOW] Followers monitor script injected, blacklist:', blacklistKeywords, 'lang:', selectedLang);
})();
)JS");

    return script;
}
