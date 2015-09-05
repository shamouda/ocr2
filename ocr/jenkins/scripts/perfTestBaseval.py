# Regression infrastructure uses basevalues supplied by app developer
# to normalize trend plots
# Add entry for each test -> 'test_name':value_to_normalize_against
# test_name should be same as run job name
# If you don't desire normalization , make an entry and pass
# 1 as value. If you are adding a new app , find avg. runtime by
# running app locally and use it.

perftest_baseval={
        'event0StickyCreate':1543402,
}



