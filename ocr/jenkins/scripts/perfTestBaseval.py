# Regression infrastructure uses basevalues supplied by app developer
# to normalize trend plots
# Add entry for each test -> 'test_name':value_to_normalize_against
# test_name should be same as run job name
# If you don't desire normalization , make an entry and pass
# 1 as value. If you are adding a new app , find avg. runtime by
# running app locally and use it.

perftest_baseval={
    # No asserts / No debug log / -O3
    'edtCreateFinishSync':941053.731,
    'edtCreateLatchSync':3076775.893,
    'edtTemplate0Create':8547561.224,
    'edtTemplate0Destroy':8121370.639,
    'event0LatchCreate':2333595.823,
    'event0LatchDestroy':3903078.013,
    'event0OnceCreate':2370265.475,
    'event0OnceDestroy':3791176.056,
    'event0StickyCreate':2301201.077,
    'event0StickyDestroy':3878393.920,
    'event1OnceFanOutEdtAddDep':583932.385,
    'event1OnceFanOutEdtSatisfy':824729.769,
    'event1StickyFanOutEdtAddDep':516823.649,
    'event1StickyFanOutEdtSatisfy':1045331.010,
    'event2LatchFanOutLatchAddDep':701891.820,
    'event2LatchFanOutLatchSatisfy':1776888.635,
    'event2OnceFanOutOnceAddDep':737806.489,
    'event2OnceFanOutOnceSatisfy':799963.631,
    'event2StickyFanOutStickyAddDep':708183.636,
    'event2StickyFanOutStickySatisfy':2748429.098,
}
