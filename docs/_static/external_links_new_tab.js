// Open links that leave the misuka documentation in a new tab.
//
// Covers both the page body and furo's sidebar, including toctree entries that
// point at absolute URLs (the upstream Mitsuba links in key_topics.rst and the
// Mitsuba how-to guides in the sidebar).

document.addEventListener('DOMContentLoaded', function () {
    document.querySelectorAll('a[href]').forEach(function (link) {
        // `link.host` is resolved against the current page, so relative links
        // and in-page anchors compare equal to the current host and are skipped.
        if (!/^https?:$/.test(link.protocol) || link.host === window.location.host)
            return;

        link.setAttribute('target', '_blank');
        link.setAttribute('rel', 'noopener noreferrer');
    });
});
