import { defineClientConfig, useRouteLocale } from '@vuepress/client'
import { computed, provide, nextTick } from 'vue'

const messages = {
  '/': {
    demo: {
      title: '示例',
      loading: '正在加载模拟器...',
      failedLoading: '加载模拟器失败',
      wheelHint: '此示例支持鼠标滚轮交互',
    },
    decl: {
      get: '读取',
      set: '设置',
      listen: '监听',
      method: '方法',
      function: '函数',
      docLink: '/framework/component/#属性文档规范',
    },
  },
  '/en/': {
    demo: {
      title: 'Demo',
      loading: 'Loading emulator...',
      failedLoading: 'Failed to load the emulator',
      wheelHint: 'This demo supports mouse wheel interaction',
    },
    decl: {
      get: 'get',
      set: 'set',
      listen: 'listen',
      method: 'method',
      function: 'function',
      docLink: '/en/framework/component/#property-documentation-specification',
    },
  },
}

const PARENT_ACTIVE_CLASS = "custom-sidebar-parent-active";
const ACTIVE_SECTION_CLASS = "custom-sidebar-active-section";

const clearParentActive = (): void => {
  document
    .querySelectorAll(`.${PARENT_ACTIVE_CLASS}`)
    .forEach((el) => el.classList.remove(PARENT_ACTIVE_CLASS));
};

const markActiveSidebarParents = (): void => {
  clearParentActive();

  document
    .querySelectorAll(`.${ACTIVE_SECTION_CLASS}`)
    .forEach((el) => el.classList.remove(ACTIVE_SECTION_CLASS));

  const sidebar = document.querySelector(".vp-sidebar");
  if (!sidebar) return;

  const activeLinks = sidebar.querySelectorAll<HTMLElement>([
    "a.route-link-active",
    "a.active",
    'a[aria-current="page"]',
    ".vp-sidebar-link.active",
    ".vp-sidebar-link.route-link-active",
  ].join(","));

  activeLinks.forEach((activeLink) => {
    const sections: HTMLElement[] = [];
    let current: HTMLElement | null = activeLink.parentElement;

    while (current && current !== sidebar) {
      if (current.tagName.toLowerCase() === "section")
        sections.push(current);
      current = current.parentElement;
    }

    if (!sections.length) return;
    // 最外层 section
    sections[sections.length - 1].classList.add(ACTIVE_SECTION_CLASS, PARENT_ACTIVE_CLASS);
    // 其它 section
    sections.slice(0, -1).forEach((section) => {
      section.classList.add(PARENT_ACTIVE_CLASS);
    });
  });
};

const updateSidebarActiveState = async (): Promise<void> => {
  await nextTick();

  // 等 Theme Hope 侧边栏 DOM 更新完成
  requestAnimationFrame(() => {
    markActiveSidebarParents();
  });
};

export default defineClientConfig({
  enhance({ app, router, siteData }) {
    if (typeof window === 'undefined')
      return

    router.afterEach(() => {
      void updateSidebarActiveState();
    });
    window.addEventListener("load", () => {
      void updateSidebarActiveState();
    });

    window.addEventListener('click', event => {
      if (window.getSelection()?.toString()) {
        // don't click <a> when selecting.
        const target = event.target as HTMLElement
        for (let i = 1; i <= 4; ++i) {
          if (target.closest(`h${i}`))
            event.preventDefault()
        }
      }
    })
    function stylesheet(url: string) {
      function a(b: string, c?: () => void) {
        let d = document.createElement('link')
        d.rel = 'stylesheet'
        d.href = b
        d.onerror = function () {
          if (typeof c === 'function')
            c()
        }
        document.head.appendChild(d)
      }
      a(url, () => a(url))
    }
    stylesheet('https://fonts.loli.net/css2?family=Noto+Serif+SC:wght@460;560;640&display=swap')

    var _hmt = _hmt || [];
    (function () {
      var hm = document.createElement("script");
      hm.src = "https://hm.baidu.com/hm.js?95fd73089e3472a5361947a28a6c2eed";
      var s = document.getElementsByTagName("script")[0];
      s.parentNode?.insertBefore(hm, s);
    })();
  },
  setup() {
    const routeLocale = useRouteLocale()
    const t = computed(() => messages[routeLocale.value] ?? messages['/'])
    provide('i18nDict', t)
  },
  rootComponents: [],
})
