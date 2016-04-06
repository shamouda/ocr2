# Regression infrastructure uses basevalues supplied by app developer
# to normalize trend plots
# Add entry for each test -> 'test_name':value_to_normalize_against
# test_name should be same as run job name
# If you don't desire normalization , make an entry and pass
# 1 as value. If you are adding a new app , find avg. runtime by
# running app locally and use it.

perftest_baseval={
    # Based on perf mode parameters as defined in the task 'ocr-build-x86-perfs'
    'edtCreateFinishSync':1807396.589,
    'edtCreateLatchSync':3479379.111,
    'edtTemplate0Create':10737728.956,
    'edtTemplate0Destroy':11530733.729,
    'event0LatchCreate':7029402.585,
    'event0LatchDestroy':9995951.639,
    'event0OnceCreate':7021116.006,
    'event0OnceDestroy':10184844.747,
    'event0StickyCreate':7026473.644,
    'event0StickyDestroy':8026596.931,
    'event1OnceFanOutEdtAddDep':1747476.534,
    'event1OnceFanOutEdtSatisfy':1448742.581,
    'event1StickyFanOutEdtAddDep':1575203.772,
    'event1StickyFanOutEdtSatisfy':1554575.913,
    'event2LatchFanOutLatchAddDep':3101130.982,
    'event2LatchFanOutLatchSatisfy':3924442.704,
    'event2OnceFanOutOnceAddDep':2375797.277,
    'event2OnceFanOutOnceSatisfy':2114415.237,
    'event2StickyFanOutStickyAddDep':2280467.039,
    'event2StickyFanOutStickySatisfy':3396084.484,
}
