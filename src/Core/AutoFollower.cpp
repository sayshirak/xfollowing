#include "AutoFollower.h"
#include "Utils/LanguageFilter.h"

AutoFollower::AutoFollower(QObject* parent)
    : QObject(parent) {
}

QString AutoFollower::getFollowScript(const QString& selectedLang) {
    QString selectedJson = LanguageFilter::buildSelectedLangJson(selectedLang);
    QString detectJs = LanguageFilter::jsDetectLanguageFunction();

    // Custom delimiter R"JS(...)JS" avoids MSVC QStringLiteral macro paren issues
    QString script = QString::fromUtf8(R"JS(
(function() {
    const pathParts = window.location.pathname.split('/');
    const userHandle = pathParts[1] || '';
    let retryCount = 0;
    const maxRetries = 2;
    const selectedLang = )JS");
    script += selectedJson;
    script += QString::fromUtf8(R"JS(;
)JS");
    script += detectJs;
    script += QString::fromUtf8(R"JS(

    function getProfileDisplayName() {
        const userName = document.querySelector('[data-testid="UserName"]');
        if (!userName) return '';
        const spans = userName.querySelectorAll('span');
        for (const span of spans) {
            const t = (span.innerText || '').trim();
            if (t && !t.startsWith('@')) return t;
        }
        return (userName.innerText || '').split('\n')[0].trim();
    }

    function getProfileBio() {
        const desc = document.querySelector('[data-testid="UserDescription"]');
        return desc ? (desc.innerText || '').trim() : '';
    }

    function checkLanguageGate() {
        const displayName = getProfileDisplayName();
        const bio = getProfileBio();
        const ok = xfollowPassesLanguageFilter(displayName, bio, selectedLang);
        if (!ok) {
            console.log('[XFOLLOW] Language gate failed name=' + displayName + ' bioLen=' + (bio ? bio.length : 0));
            console.log('XFOLLOWING_FOLLOW_SKIP_LANG:' + userHandle);
            return false;
        }
        return true;
    }

    function checkIfAccountSuspended() {
        const bodyText = document.body.innerText.toLowerCase();
        const restrictionKeywords = [
            'account suspended',
            'this account has been suspended',
            'caution: this account is temporarily restricted',
            'this account is temporarily restricted',
            'account has been withheld',
            'this account is suspended',
            'account withheld in'
        ];
        for (const keyword of restrictionKeywords) {
            if (bodyText.includes(keyword)) {
                console.log('[XFOLLOW] Detected restriction: ' + keyword);
                return true;
            }
        }
        if (bodyText.includes('this account doesn') && bodyText.includes('exist')) {
            console.log('[XFOLLOW] Detected: account does not exist');
            return true;
        }
        if (bodyText.includes('try searching for another')) {
            console.log('[XFOLLOW] Detected: try searching for another');
            return true;
        }
        return false;
    }

    function checkIfUserFollowsMe() {
        const spans = document.querySelectorAll('span');
        for (const span of spans) {
            const text = span.innerText.toLowerCase();
            if (text === 'follows you' || text === '正在关注你' || text === '关注了你') {
                return true;
            }
        }
        return false;
    }

    function checkIfOwnProfile() {
        const editProfileLink = document.querySelector('a[href="/settings/profile"]') ||
                                document.querySelector('a[href*="/settings/profile"]');
        if (editProfileLink) {
            return true;
        }
        const buttons = document.querySelectorAll('a, button, [role="button"]');
        for (const btn of buttons) {
            const text = btn.innerText.toLowerCase();
            if (text === 'edit profile' || text === '编辑个人资料' || text === 'set up profile') {
                return true;
            }
        }
        return false;
    }

    function checkIfFollowing() {
        const buttons = document.querySelectorAll('[role="button"]');
        for (const btn of buttons) {
            const testId = btn.getAttribute('data-testid');
            const btnText = btn.innerText.toLowerCase();
            if (testId && testId.includes('-unfollow')) {
                return true;
            }
            if (btnText === 'following' || btnText === '正在关注') {
                return true;
            }
        }
        return false;
    }

    function findFollowButton() {
        const buttons = document.querySelectorAll('[role="button"]');
        for (const btn of buttons) {
            const testId = btn.getAttribute('data-testid');
            if (testId && testId.includes('-follow') && !testId.includes('-unfollow')) {
                const btnText = btn.innerText.toLowerCase();
                if (btnText === 'follow' || btnText === '关注' || btnText === 'follow back' || btnText === '回关') {
                    return btn;
                }
            }
        }
        return null;
    }

    function findAndClickFollow() {
        if (checkIfAccountSuspended()) {
            console.log('[XFOLLOW] Account is suspended, skipping...');
            console.log('XFOLLOWING_ACCOUNT_SUSPENDED:' + userHandle);
            return;
        }
        if (checkIfOwnProfile()) {
            console.log('XFOLLOWING_ALREADY_FOLLOWING:' + userHandle);
            return;
        }
        if (checkIfFollowing()) {
            console.log('XFOLLOWING_ALREADY_FOLLOWING:' + userHandle);
            return;
        }
        if (!checkLanguageGate()) {
            return;
        }
        if (checkIfUserFollowsMe()) {
            console.log('[XFOLLOW] User already follows me, will follow back');
        }
        const btn = findFollowButton();
        if (btn) {
            console.log('[XFOLLOW] Found follow button, clicking... (attempt ' + (retryCount + 1) + ')');
            btn.click();
            setTimeout(verifyFollowSuccess, 2000);
        } else {
            if (checkIfOwnProfile()) {
                console.log('XFOLLOWING_ALREADY_FOLLOWING:' + userHandle);
            } else {
                console.log('XFOLLOWING_FOLLOW_FAILED:' + userHandle);
            }
        }
    }

    function verifyFollowSuccess() {
        if (checkIfFollowing()) {
            console.log('XFOLLOWING_FOLLOW_SUCCESS:' + userHandle);
        } else {
            retryCount++;
            if (retryCount <= maxRetries) {
                console.log('[XFOLLOW] Follow not confirmed, retrying... (' + retryCount + '/' + maxRetries + ')');
                setTimeout(findAndClickFollow, 1500);
            } else {
                console.log('[XFOLLOW] Follow failed after ' + maxRetries + ' retries');
                console.log('XFOLLOWING_FOLLOW_FAILED:' + userHandle);
            }
        }
    }

    function waitForPageReady() {
        let checkCount = 0;
        const maxChecks = 30;
        const interval = setInterval(() => {
            checkCount++;
            const isDocumentReady = document.readyState === 'complete';
            if (isDocumentReady && checkIfAccountSuspended()) {
                clearInterval(interval);
                console.log('[XFOLLOW] Account suspended/not exist, skipping...');
                console.log('XFOLLOWING_ACCOUNT_SUSPENDED:' + userHandle);
                return;
            }
            const isOwnProfile = checkIfOwnProfile();
            if (isOwnProfile && isDocumentReady) {
                clearInterval(interval);
                console.log('[XFOLLOW] This is own profile, skipping...');
                console.log('XFOLLOWING_ALREADY_FOLLOWING:' + userHandle);
                return;
            }
            const hasAvatar = document.querySelector('[data-testid="UserAvatar-Container-unknown"]') ||
                              document.querySelector('a[href="/' + userHandle + '/photo"]') ||
                              document.querySelector('[data-testid="UserName"]');
            const hasFollowButton = findFollowButton() !== null || checkIfFollowing();
            const hasUserProfile = document.querySelector('[data-testid="UserDescription"]') ||
                                   document.querySelector('[data-testid="UserProfileHeader_Items"]');
            console.log('[XFOLLOW] Waiting for page... check ' + checkCount + '/' + maxChecks +
                        ' ready=' + isDocumentReady +
                        ' avatar=' + !!hasAvatar +
                        ' followBtn=' + hasFollowButton +
                        ' profile=' + !!hasUserProfile);
            const isPageReady = isDocumentReady && (hasAvatar || hasUserProfile) && hasFollowButton;
            if (isPageReady) {
                clearInterval(interval);
                console.log('[XFOLLOW] Page ready, waiting 2 seconds before clicking...');
                setTimeout(findAndClickFollow, 2000);
            } else if (checkCount >= maxChecks) {
                clearInterval(interval);
                if (checkIfAccountSuspended()) {
                    console.log('[XFOLLOW] Account suspended/not exist (timeout), skipping...');
                    console.log('XFOLLOWING_ACCOUNT_SUSPENDED:' + userHandle);
                } else {
                    console.log('[XFOLLOW] Page load timeout, trying anyway...');
                    setTimeout(findAndClickFollow, 1500);
                }
            }
        }, 500);
    }

    console.log('[XFOLLOW] Auto-follow script injected, waiting for page to load...');
    waitForPageReady();
})();
)JS");

    return script;
}

QString AutoFollower::getCheckFollowBackScript() {
    QString script = R"(
(function() {
    const pathParts = window.location.pathname.split('/');
    const userHandle = pathParts[1] || '';
    let checkCount = 0;
    const maxChecks = 20;  // 最多等待10秒

    function checkIfAccountSuspended() {
        const bodyText = document.body.innerText.toLowerCase();
        const restrictionKeywords = [
            'account suspended',
            'this account has been suspended',
            'caution: this account is temporarily restricted',
            'this account is temporarily restricted',
            'account has been withheld',
            'this account is suspended',
            'account withheld in'
        ];
        for (const keyword of restrictionKeywords) {
            if (bodyText.includes(keyword)) {
                console.log('[XFOLLOW] Detected restriction: ' + keyword);
                return true;
            }
        }
        if (bodyText.includes('this account doesn') && bodyText.includes('exist')) {
            console.log('[XFOLLOW] Detected: account does not exist');
            return true;
        }
        if (bodyText.includes('try searching for another')) {
            console.log('[XFOLLOW] Detected: try searching for another');
            return true;
        }
        return false;
    }

    function checkIfUserFollowsMe() {
        const spans = document.querySelectorAll('span');
        for (const span of spans) {
            const text = span.innerText.toLowerCase();
            if (text === 'follows you' || text === '正在关注你' || text === '关注了你') {
                return true;
            }
        }
        return false;
    }

    function checkIfIFollowThem() {
        const buttons = document.querySelectorAll('[role="button"]');
        for (const btn of buttons) {
            const testId = btn.getAttribute('data-testid');
            if (testId && testId.includes('-unfollow')) {
                return true;
            }
            const btnText = btn.innerText.toLowerCase();
            if (btnText === 'following' || btnText === '正在关注') {
                return true;
            }
        }
        return false;
    }

    function doCheck() {
        checkCount++;
        const isDocumentReady = document.readyState === 'complete';

        if (isDocumentReady && checkIfAccountSuspended()) {
            clearInterval(interval);
            console.log('XFOLLOWING_CHECK_SUSPENDED:' + userHandle);
            return;
        }

        const hasUserName = document.querySelector('[data-testid="UserName"]');
        const hasFollowButton = document.querySelector('[role="button"]');

        if (isDocumentReady && hasUserName && hasFollowButton) {
            clearInterval(interval);

            // 等待2秒确保页面稳定
            setTimeout(() => {
                if (checkIfAccountSuspended()) {
                    console.log('XFOLLOWING_CHECK_SUSPENDED:' + userHandle);
                    return;
                }

                const iFollowThem = checkIfIFollowThem();
                const theyFollowMe = checkIfUserFollowsMe();

                console.log('[XFOLLOW] Check result: iFollowThem=' + iFollowThem + ' theyFollowMe=' + theyFollowMe);

                if (!iFollowThem) {
                    console.log('XFOLLOWING_CHECK_NOT_FOLLOWING:' + userHandle);
                } else if (theyFollowMe) {
                    console.log('XFOLLOWING_CHECK_FOLLOWS_BACK:' + userHandle);
                } else {
                    console.log('XFOLLOWING_CHECK_NOT_FOLLOW_BACK:' + userHandle);
                }
            }, 2000);
            return;
        }

        if (checkCount >= maxChecks) {
            clearInterval(interval);
            console.log('[XFOLLOW] Check timeout, assuming not following back');
            console.log('XFOLLOWING_CHECK_NOT_FOLLOW_BACK:' + userHandle);
        }
    }

    const interval = setInterval(doCheck, 500);
    doCheck();
})();
)";

    return script;
}

QString AutoFollower::getUnfollowScript() {
    QString script = R"(
(function() {
    const pathParts = window.location.pathname.split('/');
    const userHandle = pathParts[1] || '';
    let retryCount = 0;
    const maxRetries = 2;

    function checkIfAccountSuspended() {
        const bodyText = document.body.innerText.toLowerCase();
        const restrictionKeywords = [
            'account suspended',
            'this account has been suspended',
            'caution: this account is temporarily restricted',
            'this account is temporarily restricted',
            'account has been withheld',
            'this account is suspended',
            'account withheld in'
        ];
        for (const keyword of restrictionKeywords) {
            if (bodyText.includes(keyword)) {
                console.log('[XFOLLOW] Detected restriction: ' + keyword);
                return true;
            }
        }
        if (bodyText.includes('this account doesn') && bodyText.includes('exist')) {
            console.log('[XFOLLOW] Detected: account does not exist');
            return true;
        }
        if (bodyText.includes('try searching for another')) {
            console.log('[XFOLLOW] Detected: try searching for another');
            return true;
        }
        return false;
    }

    function checkIfFollowing() {
        const buttons = document.querySelectorAll('[role="button"]');
        for (const btn of buttons) {
            const testId = btn.getAttribute('data-testid');
            const btnText = btn.innerText.toLowerCase();
            if (testId && testId.includes('-unfollow')) {
                return true;
            }
            if (btnText === 'following' || btnText === '正在关注') {
                return true;
            }
        }
        return false;
    }

    function findUnfollowButton() {
        const buttons = document.querySelectorAll('[role="button"]');
        for (const btn of buttons) {
            const testId = btn.getAttribute('data-testid');
            if (testId && testId.includes('-unfollow')) {
                return btn;
            }
            const btnText = btn.innerText.toLowerCase();
            if (btnText === 'following' || btnText === '正在关注') {
                return btn;
            }
        }
        return null;
    }

    function findConfirmButton() {
        const buttons = document.querySelectorAll('[role="button"]');
        for (const btn of buttons) {
            const testId = btn.getAttribute('data-testid');
            const btnText = btn.innerText.toLowerCase();
            if (testId === 'confirmationSheetConfirm' ||
                btnText === 'unfollow' || btnText === '取消关注') {
                return btn;
            }
        }
        return null;
    }

    function doUnfollow() {
        if (checkIfAccountSuspended()) {
            console.log('XFOLLOWING_ACCOUNT_SUSPENDED:' + userHandle);
            return;
        }

        if (!checkIfFollowing()) {
            console.log('[XFOLLOW] Not following, nothing to unfollow');
            console.log('XFOLLOWING_UNFOLLOW_SUCCESS:' + userHandle);
            return;
        }

        const btn = findUnfollowButton();
        if (btn) {
            console.log('[XFOLLOW] Found unfollow button, clicking...');
            btn.click();

            // 等待确认对话框
            setTimeout(() => {
                const confirmBtn = findConfirmButton();
                if (confirmBtn) {
                    console.log('[XFOLLOW] Found confirm button, clicking...');
                    confirmBtn.click();
                    setTimeout(verifyUnfollowSuccess, 2000);
                } else {
                    // 没有确认对话框，直接验证
                    setTimeout(verifyUnfollowSuccess, 1000);
                }
            }, 1000);
        } else {
            console.log('XFOLLOWING_UNFOLLOW_FAILED:' + userHandle);
        }
    }

    function verifyUnfollowSuccess() {
        if (!checkIfFollowing()) {
            console.log('XFOLLOWING_UNFOLLOW_SUCCESS:' + userHandle);
        } else {
            retryCount++;
            if (retryCount <= maxRetries) {
                console.log('[XFOLLOW] Unfollow not confirmed, retrying... (' + retryCount + '/' + maxRetries + ')');
                setTimeout(doUnfollow, 1500);
            } else {
                console.log('[XFOLLOW] Unfollow failed after ' + maxRetries + ' retries');
                console.log('XFOLLOWING_UNFOLLOW_FAILED:' + userHandle);
            }
        }
    }

    // 等待页面加载
    function waitForPageReady() {
        let checkCount = 0;
        const maxChecks = 30;

        const interval = setInterval(() => {
            checkCount++;
            const isDocumentReady = document.readyState === 'complete';

            if (isDocumentReady && checkIfAccountSuspended()) {
                clearInterval(interval);
                console.log('XFOLLOWING_ACCOUNT_SUSPENDED:' + userHandle);
                return;
            }

            const hasUserName = document.querySelector('[data-testid="UserName"]');
            const hasButton = document.querySelector('[role="button"]');

            if (isDocumentReady && hasUserName && hasButton) {
                clearInterval(interval);
                setTimeout(doUnfollow, 2000);
            } else if (checkCount >= maxChecks) {
                clearInterval(interval);
                console.log('[XFOLLOW] Page load timeout, trying anyway...');
                doUnfollow();
            }
        }, 500);
    }

    console.log('[XFOLLOW] Unfollow script injected, waiting for page to load...');
    waitForPageReady();
})();
)";

    return script;
}
