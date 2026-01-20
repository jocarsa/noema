// Language Management
class LanguageManager {
    constructor() {
        this.currentLang = 'en';
        this.translations = {};
        this.init();
    }

    async init() {
        // Load default language
        await this.loadLanguage('en');
        
        // Set up language selector
        const langSelect = document.getElementById('languageSelect');
        if (langSelect) {
            langSelect.value = this.currentLang;
            langSelect.addEventListener('change', (e) => {
                this.changeLanguage(e.target.value);
            });
        }
        
        // Set up mobile menu toggle
        const navToggle = document.querySelector('.nav-toggle');
        const navMenu = document.querySelector('.nav-menu');
        
        if (navToggle && navMenu) {
            navToggle.addEventListener('click', () => {
                navMenu.classList.toggle('active');
            });
        }
        
        // Smooth scrolling for navigation links
        document.querySelectorAll('a[href^="#"]').forEach(anchor => {
            anchor.addEventListener('click', function (e) {
                e.preventDefault();
                const targetId = this.getAttribute('href');
                if (targetId === '#') return;
                
                const targetElement = document.querySelector(targetId);
                if (targetElement) {
                    // Close mobile menu if open
                    if (navMenu) {
                        navMenu.classList.remove('active');
                    }
                    
                    window.scrollTo({
                        top: targetElement.offsetTop - 80,
                        behavior: 'smooth'
                    });
                }
            });
        });
        
        // Initialize code highlighting
        this.initCodeHighlighting();
    }

    async loadLanguage(lang) {
        try {
            const response = await fetch(`lang/${lang}.json`);
            if (!response.ok) throw new Error('Language file not found');
            
            this.translations[lang] = await response.json();
            this.currentLang = lang;
            this.applyTranslations();
            
            // Update code highlighting
            setTimeout(() => {
                document.querySelectorAll('pre code').forEach((block) => {
                    hljs.highlightElement(block);
                });
            }, 100);
        } catch (error) {
            console.error('Error loading language:', error);
            // Fallback to English
            if (lang !== 'en') {
                await this.loadLanguage('en');
            }
        }
    }

    changeLanguage(lang) {
        if (lang !== this.currentLang) {
            this.loadLanguage(lang);
        }
    }

    applyTranslations() {
        const t = this.translations[this.currentLang];
        if (!t) return;

        // Update meta tags
        document.title = t.meta.title;
        document.querySelector('meta[name="description"]')?.setAttribute('content', t.meta.description);

        // Update navigation
        this.updateElementText('nav-home', t.nav.home);
        this.updateElementText('nav-features', t.nav.features);
        this.updateElementText('nav-syntax', t.nav.syntax);
        this.updateElementText('nav-examples', t.nav.examples);
        this.updateElementText('nav-download', t.nav.download);
        this.updateElementText('nav-docs', t.nav.docs);
        this.updateElementText('nav-github', t.nav.github);
        this.updateElementText('nav-title', 'Noema');

        // Update hero section
        this.updateElementText('hero-title', t.hero.title);
        this.updateElementText('hero-subtitle', t.hero.subtitle);
        this.updateElementText('hero-cta1', t.hero.cta1);
        this.updateElementText('hero-cta2', t.hero.cta2);

        // Update features
        this.updateElementText('features-title', t.features.title);
        this.renderFeatures(t.features.items);

        // Update syntax
        this.updateElementText('syntax-title', t.syntax.title);
        this.updateElementText('syntax-code1-title', t.syntax.code1.title);
        this.updateElementText('syntax-code2-title', t.syntax.code2.title);
        this.updateElementText('syntax-code3-title', t.syntax.code3.title);
        this.updateElementText('syntax-code1', t.syntax.code1.code);
        this.updateElementText('syntax-code2', t.syntax.code2.code);
        this.updateElementText('syntax-code3', t.syntax.code3.code);

        // Update examples
        this.updateElementText('examples-title', t.examples.title);
        this.renderExamples(t.examples.items);

        // Update keywords
        this.updateElementText('keywords-title', t.keywords.title);
        this.renderKeywords(t.keywords.table);

        // Update download
        this.updateElementText('download-title', t.download.title);
        this.updateElementText('download-description', t.download.description);
        this.updateElementText('download-version', t.download.version);
        this.updateElementText('download-btn-source', t.download.btn_source);
        this.updateElementText('download-btn-binary', t.download.btn_binary);
        this.updateElementText('download-requirements', t.download.requirements);

        // Update footer
        this.updateElementText('footer-description', t.footer.description);
        this.updateElementText('footer-copyright', t.footer.copyright);
        this.renderFooterLinks(t.footer.links);
    }

    updateElementText(elementId, text) {
        const element = document.getElementById(elementId);
        if (element) {
            element.textContent = text;
        }
    }

    renderFeatures(features) {
        const container = document.getElementById('features-items');
        if (!container) return;

        container.innerHTML = features.map(feature => `
            <div class="feature-card">
                <i class="fas fa-star"></i>
                <h3>${feature.title}</h3>
                <p>${feature.description}</p>
            </div>
        `).join('');
    }

    renderExamples(examples) {
        const container = document.getElementById('examples-items');
        if (!container) return;

        container.innerHTML = examples.map(example => `
            <div class="example-card">
                <h3>${example.title}</h3>
                <pre><code class="language-noema">${example.code}</code></pre>
            </div>
        `).join('');
    }

    renderKeywords(keywords) {
        const container = document.getElementById('keywords-table');
        if (!container) return;

        container.innerHTML = keywords.map(keyword => `
            <tr>
                <td>${keyword[0]}</td>
                <td>${keyword[1]}</td>
            </tr>
        `).join('');
    }

    renderFooterLinks(links) {
        const container = document.getElementById('footer-links');
        if (!container) return;

        container.innerHTML = links.map(link => `
            <li><a href="#">${link}</a></li>
        `).join('');
    }

    initCodeHighlighting() {
        // Custom Noema syntax highlighting
        hljs.registerLanguage('noema', function(hljs) {
            const KEYWORDS = {
                keyword: 'si alio pro dum munus redit verum falsum nulla et aut non in',
                literal: 'verum falsum nulla'
            };
            
            const STRINGS = {
                className: 'string',
                variants: [
                    hljs.QUOTE_STRING_MODE,
                    hljs.APOS_STRING_MODE
                ]
            };
            
            const NUMBERS = {
                className: 'number',
                variants: [
                    { begin: '\\b(0[bB][01]+)' },
                    { begin: '\\b(0[oO][0-7]+)' },
                    { begin: hljs.C_NUMBER_RE }
                ]
            };
            
            const COMMENTS = {
                className: 'comment',
                variants: [
                    { begin: '#', end: '$' }
                ]
            };
            
            return {
                name: 'Noema',
                aliases: ['noema'],
                keywords: KEYWORDS,
                contains: [
                    STRINGS,
                    NUMBERS,
                    COMMENTS,
                    {
                        className: 'function',
                        beginKeywords: 'munus',
                        end: ':',
                        contains: [
                            hljs.UNDERSCORE_TITLE_MODE
                        ]
                    },
                    {
                        className: 'built_in',
                        begin: 'sonus\\.'
                    }
                ]
            };
        });
        
        // Highlight all code blocks
        document.querySelectorAll('pre code').forEach((block) => {
            hljs.highlightElement(block);
        });
    }
}

// Initialize the application
document.addEventListener('DOMContentLoaded', () => {
    window.languageManager = new LanguageManager();
    
    // Add smooth scroll behavior
    document.documentElement.style.scrollBehavior = 'smooth';
    
    // Add scroll-to-top button
    const scrollToTopBtn = document.createElement('button');
    scrollToTopBtn.innerHTML = '<i class="fas fa-arrow-up"></i>';
    scrollToTopBtn.className = 'scroll-to-top';
    scrollToTopBtn.style.cssText = `
        position: fixed;
        bottom: 30px;
        right: 30px;
        width: 50px;
        height: 50px;
        background-color: var(--primary-color);
        color: white;
        border: none;
        border-radius: 50%;
        cursor: pointer;
        display: none;
        align-items: center;
        justify-content: center;
        font-size: 1.2rem;
        box-shadow: var(--shadow);
        transition: var(--transition);
        z-index: 999;
    `;
    
    document.body.appendChild(scrollToTopBtn);
    
    scrollToTopBtn.addEventListener('click', () => {
        window.scrollTo({ top: 0, behavior: 'smooth' });
    });
    
    window.addEventListener('scroll', () => {
        if (window.pageYOffset > 300) {
            scrollToTopBtn.style.display = 'flex';
        } else {
            scrollToTopBtn.style.display = 'none';
        }
    });
    
    // Add hover effect to feature cards
    document.querySelectorAll('.feature-card, .code-card, .example-card').forEach(card => {
        card.addEventListener('mouseenter', () => {
            card.style.transform = 'translateY(-5px)';
        });
        
        card.addEventListener('mouseleave', () => {
            card.style.transform = 'translateY(0)';
        });
    });
});
