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

QString PostMonitor::getMonitorScript(const QList<Keyword>& keywords, const QString& selectedLang) {
    QString keywordsJson = buildKeywordsArray(keywords);
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
    const selectedLang = )JS");
    script += selectedJson;
    script += QString::fromUtf8(R"JS(;
)JS");
    script += detectJs;
    script += QString::fromUtf8(R"JS(

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

    function parsePost(article) {
        try {
            const authorLinks = article.querySelectorAll('a[href^="/"]');
            let authorHandle = '';
            let authorName = '';

            for (const link of authorLinks) {
                const href = link.getAttribute('href');
                if (href && href.match(/^\/[a-zA-Z0-9_]+$/) && !href.includes('/status/')) {
                    authorHandle = href.substring(1);
                    authorName = link.innerText || authorHandle;
                    break;
                }
            }

            if (!authorHandle) return null;

            const verifiedBadge = article.querySelector('[data-testid="icon-verified"]') ||
                                  article.querySelector('svg[aria-label="Verified account"]') ||
                                  article.querySelector('svg[aria-label="已认证帐号"]') ||
                                  article.querySelector('[aria-label="Verified account"]') ||
                                  article.querySelector('[aria-label="已认证帐号"]');
            if (!verifiedBadge) return null;

            const contentDiv = article.querySelector('[data-testid="tweetText"]');
            const content = contentDiv ? contentDiv.innerText : '';
            if (!content) return null;

            let matchedKeyword = null;
            for (const kw of keywords) {
                if (content.toLowerCase().includes(kw.toLowerCase())) {
                    matchedKeyword = kw;
                    break;
                }
            }
            if (!matchedKeyword) return null;

            const statusLink = article.querySelector('a[href*="/status/"]');
            const postUrl = statusLink ? 'https://x.com' + statusLink.getAttribute('href') : '';
            const postId = postUrl.split('/status/')[1]?.split('?')[0] || '';
            if (!postId) return null;

            const timeElement = article.querySelector('time');
            const postTime = timeElement ? timeElement.getAttribute('datetime') : '';

            const bio = extractBioFromArticle(article);
            const langOk = xfollowPassesLanguageFilter(authorName, bio, selectedLang);
            if (!langOk) return null;

            return {
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
            };
        } catch (e) {
            console.log('[XFOLLOW] Parse error:', e);
            return null;
        }
    }

    function scanPosts() {
        const articles = document.querySelectorAll('article[data-testid="tweet"]');
        const newPosts = [];

        articles.forEach(article => {
            const statusLink = article.querySelector('a[href*="/status/"]');
            const postId = statusLink?.getAttribute('href')?.split('/status/')[1]?.split('?')[0];

            if (postId && !window.xfollowingProcessedIds.has(postId)) {
                window.xfollowingProcessedIds.add(postId);
                const post = parsePost(article);
                if (post) {
                    newPosts.push(post);
                }
            }
        });

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
            scanPosts();
            if (scanCount < maxScans) setTimeout(doScan, 1500);
        }
        setTimeout(doScan, 2000);
    }

    if (window.xfollowingInterval) clearInterval(window.xfollowingInterval);
    window.xfollowingInterval = setInterval(scanPosts, 5000);

    window.xfollowingObserver = new MutationObserver(() => { scanPosts(); });
    window.xfollowingObserver.observe(document.body, { childList: true, subtree: true });

    initialScan();
    console.log('[XFOLLOW] Monitor script injected, keywords:', keywords, 'lang:', selectedLang);
})();
)JS");

    return script;
}

QString PostMonitor::getFollowersMonitorScript(const QString& selectedLang) {
    QString selectedJson = LanguageFilter::buildSelectedLangJson(selectedLang);
    QString detectJs = LanguageFilter::jsDetectLanguageFunction();

    QString script = QString::fromUtf8(R"JS(
(function() {
    if (!window.xfollowingFollowersProcessedIds) {
        window.xfollowingFollowersProcessedIds = new Set();
    }

    const selectedLang = )JS");
    script += selectedJson;
    script += QString::fromUtf8(R"JS(;
)JS");
    script += detectJs;
    script += QString::fromUtf8(R"JS(

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

    function parseFollower(userCell) {
        try {
            const userLinks = userCell.querySelectorAll('a[href^="/"]');
            let userHandle = '';
            let userName = '';

            for (const link of userLinks) {
                const href = link.getAttribute('href');
                if (href && href.match(/^\/[a-zA-Z0-9_]+$/) && !href.includes('/status/')) {
                    userHandle = href.substring(1);
                    const nameSpan = link.querySelector('span');
                    userName = nameSpan ? nameSpan.innerText : userHandle;
                    const userNameBlock = userCell.querySelector('[data-testid="UserName"]');
                    if (userNameBlock) {
                        const firstSpan = userNameBlock.querySelector('span');
                        if (firstSpan && firstSpan.innerText) userName = firstSpan.innerText.trim();
                    }
                    break;
                }
            }

            if (!userHandle) return null;

            const verifiedBadge = userCell.querySelector('[data-testid="icon-verified"]') ||
                                  userCell.querySelector('svg[aria-label="Verified account"]') ||
                                  userCell.querySelector('svg[aria-label="已认证帐号"]') ||
                                  userCell.querySelector('[aria-label="Verified account"]') ||
                                  userCell.querySelector('[aria-label="已认证帐号"]');
            if (!verifiedBadge) return null;

            const allSpans = userCell.querySelectorAll('span');
            for (const span of allSpans) {
                const text = span.innerText.toLowerCase().trim();
                if (text === 'follows you' || text.includes('follows you') ||
                    text === '关注了你' || text.includes('关注了你')) {
                    return null;
                }
            }

            const bio = extractBioFromUserCell(userCell);
            const langOk = xfollowPassesLanguageFilter(userName, bio, selectedLang);
            if (!langOk) return null;

            return {
                authorHandle: userHandle,
                authorName: userName,
                authorUrl: 'https://x.com/' + userHandle,
                bio: langOk.bio,
                nameLang: langOk.nameLang,
                bioLang: langOk.bioLang
            };
        } catch (e) {
            console.log('[XFOLLOW] Parse follower error:', e);
            return null;
        }
    }

    function scanFollowers() {
        const userCells = document.querySelectorAll('[data-testid="UserCell"]');
        const newFollowers = [];

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

            if (userHandle && !window.xfollowingFollowersProcessedIds.has(userHandle)) {
                window.xfollowingFollowersProcessedIds.add(userHandle);
                const follower = parseFollower(cell);
                if (follower) newFollowers.push(follower);
            }
        });

        if (newFollowers.length > 0) {
            console.log('XFOLLOWING_NEW_FOLLOWERS:' + JSON.stringify(newFollowers));
        }
    }

    if (window.xfollowingFollowersInterval) clearInterval(window.xfollowingFollowersInterval);
    window.xfollowingFollowersInterval = setInterval(scanFollowers, 3000);
    setTimeout(scanFollowers, 2000);
    console.log('[XFOLLOW] Followers monitor script injected, lang:', selectedLang);
})();
)JS");

    return script;
}
