module.exports = {
  extends: ['standard'],
  // plugins: ['prettier'],
  env: {
    browser: true
    // node: true
  },
  settings: {
    rules: {
      indent: ['error', 4, { ignoredNodes: [] }]
    }
  },
  rules: {
    // 'prettier/prettier' : ['error', {endOfLine : 'auto'}, {usePrettierrc : true}]
  }
}
