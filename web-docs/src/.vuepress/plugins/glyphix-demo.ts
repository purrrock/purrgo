import * as fs from 'fs/promises'
import * as path from 'path'
import {exec} from 'child_process'
import {App, Plugin} from 'vuepress'
import * as chokidar from 'chokidar'
import { colors, logger } from 'vuepress/utils'

interface UxFile {
  path: string,
  content: string
}

interface Applet {
  path: string,
  srcPath: string,
  uxFiles: UxFile[]
}

const watchPaths: string[] = [
  'api',
  'components',
  'framework',
  'tutorials',
  'cookbook'
]

export const glyphixDemoPlugin: Plugin = {
  name: 'vuepress-plugin-glyphix-demo',
  async onInitialized(app: App) {
    try {
      const assetsPath = path.join(app.dir.temp(), 'glyphix-demo')
      await fs.access(assetsPath, fs.constants.R_OK)
      await fs.rm(path.join(app.dir.temp(), 'glyphix-demo'))
    } catch {
    }

    if (!app.env.isDev) {
      const dirs = watchPaths.map(x => path.join(app.dir.source(), x))
      for (const dir of dirs) {
        const files = await fs.readdir(dir, {
          recursive: true,
          withFileTypes: true
        })
        for (const f of files)
          if (f.isFile() && path.extname(f.name) == '.md')
            await demoApplet(app, path.join(f.path, f.name))
      }
    }
  },

  async onWatched(app: App, watchers): Promise<void> {
    let tasks: (() => Promise<void>)[] = []
    const watcher = chokidar.watch(watchPaths.map(x => path.join(app.dir.source(), x)))

    watcher.on('all',
      async (event: string, p: string) => {
        if (event == 'add' || event == 'change') {
          tasks.push(async () => await demoApplet(app, p))
          if (tasks.length == 1) {
            while (tasks.length) { // process tasks
              try { await tasks[0]() } catch (e) {} // skip process if failed
              tasks.shift()
            }
          }
        }
      });
    watchers.push(watcher)
  }
}

async function demoApplet(app: App, mdFile: string): Promise<void> {
  if (path.extname(mdFile) == '.md') {
    const applets = await generateUxContent(mdFile)
    for (const applet of applets) {
      const dstPath = path.join(app.dir.public(), 'res', 'demo')
      const project = path.join(app.dir.temp(), 'glyphix-demo')
      await generateApplet(dstPath, project, applet)
    }
  }
}

async function generateApplet(resPath: string, basePath: string, applet: Applet): Promise<void> {
  const appPath = await createApplet(basePath, applet)
  const promise = new Promise<void>((resolve, reject) => {
    exec(
      'gx build -e', {
        cwd: appPath
      },
      (error, stdout, stderr) => {
        if (error !== null) {
          console.log(stdout)
          console.log(stderr)
          logger.error(`${colors.yellow('gx build failed')}: ${applet.srcPath}`)
          reject()
        }
        resolve()
      })
  })
  await promise
  const src = path.join(appPath, '.glyphix-work', 'image', 'default', 'apps', 'com.example.app.pkg')
  const dst = path.join(resPath, applet.path + '.pkg')
  await fs.mkdir(path.dirname(dst), {recursive: true})
  await fs.copyFile(src, dst)
}

async function createApplet(basePath: string, applet: Applet): Promise<string> {
  const srcPath = path.join(basePath, applet.path, 'src')
  try {
    await fs.rm(path.dirname(srcPath), { recursive: true })
  } catch {}
  await fs.mkdir(path.join(srcPath), { recursive: true })
  if (applet.uxFiles.length) {
    for (const ux of applet.uxFiles) {
      const f = await fs.open(path.join(srcPath, ux.path), 'w')
      await f.writeFile(ux.content)
      await f.close()
    }

    let f = await fs.open(path.join(srcPath, 'manifest.json'), 'w')
    await f.writeFile(await generateManifest(applet.uxFiles))
    await f.close()

    f = await fs.open(path.join(srcPath, 'app.js'), 'w')
    await f.writeFile('export default{onCreate(){},onShow(){},onHide(){},onDestroy(){},onError(){},onPageNotFound(_){}}')
    await f.close()
  }

  try {
    const assetsPath = path.join(path.dirname(applet.srcPath), '.demo', applet.path, '.')
    await fs.access(assetsPath, fs.constants.R_OK)
    await fs.cp(assetsPath, srcPath, {force: true, recursive: true})
  } catch {
  }

  return path.dirname(srcPath)
}

async function generateManifest(uxFiles: UxFile[]): Promise<string> {
  let manifest: any = {
    package: 'com.example.app',
    name: 'example',
    versionName: '1.0.0',
    versionCode: 1,
    router: {
      pages: {},
    },
    config: {
      designWidth: 410,
    },
  }
  if (uxFiles.length) {
    const pageName = path.basename(uxFiles[0].path, '.ux')
    manifest.router.entry = pageName
    manifest.router.pages[pageName] = {
      path: '/',
      component: pageName
    }
  }
  return JSON.stringify(manifest)
}

async function generateUxContent(filePath: string): Promise<Applet[]> {
  const content = await fs.readFile(filePath, 'utf-8')
  const demos = content.matchAll(/<[Gg]lyphix(.*)>([\s\S]*?)<\/[Gg]lyphix>/g)
  let applets: Applet[] = []
  for (const demo of demos) {
    const src = demo[1].match(/id="([^"]*)"/)![1]
    let uxFiles: UxFile[] = []
    if (!demo[2].match(/<!-- disable-snippets -->/)) {
      const template = demo[2].match(/```\s*html([\s\S]+?)```/)
      const style = demo[2].match(/```\s*css([\s\S]+?)```/)
      const script = demo[2].match(/```\s*js([\s\S]+?)```/)
      const ux = (template ? `<template>${template[1]}</template>\n\n` : '')
        + (style ? `<style>${style[1]}</style>\n\n` : '')
        + (script ? `<script>${script[1]}</script>\n` : '')
      uxFiles = [{path: 'main.ux', content: ux}]
    }
    applets.push({
      path: src,
      srcPath: filePath,
      uxFiles: uxFiles
    })
  }
  return applets
}
