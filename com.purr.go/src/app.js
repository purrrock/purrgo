export default {
  onCreate() {
    console.info('PurrGo onCreate')
  },
  onShow() {
    console.info('PurrGo onShow')
  },
  onHide() {
    console.info('PurrGo onHide')
  },
  onDestroy() {
    console.info('PurrGo onDestroy')
  },
  onError() {
    console.log('PurrGo onError')
  },
  onPageNotFound(params) {
    const { uri = '' } = params
    console.error('error uri', uri)
  }
}
