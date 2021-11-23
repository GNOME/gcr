gi_docgen = find_program('gi-docgen')

subdir('gck')
subdir('gcr')
subdir('ui')

# Create a pseudo target that build all docs at once
alias_target('docs',
  gck_docs,
  gcr_docs,
  gcr_ui_docs,
)
