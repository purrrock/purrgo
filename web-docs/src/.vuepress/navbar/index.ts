import { navbar } from "vuepress-theme-hope";

export const zhNavbar = navbar([
  "/",
  { text: "入门", icon: "routes", link: "/tutorials/" },
  {
    text: "应用开发",
    icon: "rocket-launch",
    children: [
      { text: "框架", icon: "code-braces", link: "/framework/" },
      { text: "原生组件", icon: "code-tags", link: "/components/" },
      { text: "测试", icon: "flask", link: "/framework/testing/" },
      { text: "API", icon: "function-variant", link: "/api/" },
      { text: "实用指南", icon: "creation", link: "/cookbook/" },
    ]
  },
  { text: "C++ 开发", icon: "language-cpp", link: "/cxxdev/" },
]);

export const enNavbar = navbar([
  "/en/",
  { text: "Tutorials", icon: "routes", link: "/en/tutorials/" },
  {
    text: "App Development",
    icon: "rocket-launch",
    children: [
      { text: "Framework", icon: "code-braces", link: "/en/framework/" },
      { text: "Native Components", icon: "code-tags", link: "/en/components/" },
      { text: "Testing", icon: "flask", link: "/en/framework/testing/" },
      { text: "API", icon: "function-variant", link: "/en/api/" },
      { text: "Cookbook", icon: "creation", link: "/en/cookbook/" },
    ]
  },
  { text: "C++ Development", icon: "language-cpp", link: "/en/cxxdev/" },
]);
