gi_docgen = find_program('gi-docgen')

subdir('gck')
subdir('gcr')
subdir('gcr-gtk3')

# Create a pseudo target that build all docs at once
alias_target('docs',
  gck_docs,
  gcr_docs,
  gcr_gtk3_docs,
)
