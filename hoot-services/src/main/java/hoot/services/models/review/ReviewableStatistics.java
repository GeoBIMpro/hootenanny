package hoot.services.models.review;

import org.json.simple.JSONObject;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.mysema.query.Tuple;

public class ReviewableStatistics extends ReviewQueryMapper {
	private static final Logger log = LoggerFactory.getLogger(ReviewableStatistics.class);
	private long _totalCount = 0;
	private long _unreviewedCount = 0;
	
	public ReviewableStatistics(final long nTotal, final long nUnreviewed)
	{
		_totalCount = nTotal;
		_unreviewedCount = nUnreviewed;
	}
	
	
	public long getTotalCount()
	{
		return _totalCount;
	}
	
	public long getUnreviewedCount()
	{
		return _unreviewedCount;
	}

	public String toString()
	{
		JSONObject o = new JSONObject();
		
		
		return o.toJSONString();
	}
}