#ifndef __SEGMENTATIONTOOLS__
#define __SEGMENTATIONTOOLS__

#include "BaseTools.h"
#include <math.h>

#include <pcl/point_cloud.h>
#include <pcl/kdtree/kdtree_flann.h>

#include <pcl/point_types.h>
#include <pcl/features/normal_3d.h>

namespace SegmentationTools
	/************************************************************************/
	/* 1.angularClassifier     2.Estimating the normals*/
	/************************************************************************/
{
#pragma region angularClassifier
	float getMinZvalueofRasterCell(const LiDARBaseTools::GrideIndex* pGrideIndex,
		const int nrows,const int ncols,
		const std::vector<LiDARBaseTools::LasPoint> &inputCloud,
		const int i ,const int j)
	/*
	todo:      get mini Z value of points in a Raster cell
	date:      2015.3.9
	author:    JianPing(lijianping@whu.edu.cn)
	see:       Area-wide roof plane segmentation in airbone LiDAR
	           point clouds(2010)
	*/	
	{
		float minZ = inputCloud[pGrideIndex[i*ncols+j].ptIndex[0]].z;
		float tempZ;
		for (std::vector<int>::const_iterator it = pGrideIndex[i*ncols+j].ptIndex.begin();
			it!=pGrideIndex[i*ncols+j].ptIndex.end();++it)
		{
			tempZ = inputCloud[*it].z;
			if (minZ>tempZ)
			{
				minZ = tempZ;
			}
		}
		return minZ;
	}

	bool CalculateEchoRatio(const LiDARBaseTools::GrideIndex* pGrideIndex,
		const int nrows,const int ncols,
		const int* flag,//����㣨0���ǵ����
		const std::vector<LiDARBaseTools::LasPoint> &inputCloud,
		float* &sER_Raster,
		float threshold = 5)
	/*
	todo:      calculate the slope-adaptive Echo Ratio value, and
			   create a raster of float to store it
	date:      2015.3.9
	author:    JianPing(lijianping@whu.edu.cn)
	see:       Area-wide roof plane segmentation in airbone LiDAR
	           point clouds(2010)
	*/	
	{
		std::cout<<"create slope-adaptive Echo Ratio Raster..."<<std::endl;
		sER_Raster = new float[nrows*ncols];
		int n3D(0),n2D(0);
		float minZ;
		for (int i = 0 ; i < nrows ; ++i)
		{
			for (int j = 0 ; j < ncols ; ++j)
			{
				n2D = pGrideIndex[i*ncols+j].ptIndex.size();
				if (n2D == 0)//no data point in this cell
				{
					sER_Raster[i*ncols+j] = 0;
					continue;
				}
				
				if (flag[i*ncols+j] == 0)//�Ƿ�Ϊ�����
				{
					sER_Raster[i*ncols+j] = 0;
					continue;
				}

				minZ = SegmentationTools::getMinZvalueofRasterCell(pGrideIndex,nrows,ncols,inputCloud,i,j);


				for (std::vector<int>::const_iterator it = pGrideIndex[i*ncols+j].ptIndex.begin();
					it!=pGrideIndex[i*ncols+j].ptIndex.end();++it)
				{
					if (minZ+threshold > inputCloud[*it].z)
					{
						++n3D;
					}
				}
				sER_Raster[i*ncols+j] = n3D/float(n2D);
				n3D = 0;
			}
		}

		std::cout<<"have created slope-adaptive Echo Ratio Raster.\nDone."<<std::endl;

		return true;
	}

	bool pointIsBuilding(const pcl::PointXY &searchPoint,//��ǰ�ǵ����
		const std::vector<int> &pointIdxRadiusSearch,//�����ڵ�ID
		const std::vector<float> &pointRadiusSquaredDistance,//�����ڸ��㵽��ǰ�ǵ�������
		const std::vector<LiDARBaseTools::LasPoint> &groundPoints//�������
		)
	/*
	todo:      judge if a point belong to a building
	date:      2015.3.13
	author:    JianPing(lijianping@whu.edu.cn)
	see:       Area-wide roof plane segmentation in airbone LiDAR
	           point clouds(2010)
	*/	
	{
		const LiDARBaseTools::LasPoint* tempPoint;
		float tempAng;
		int nPoints = pointIdxRadiusSearch.size();
		float *angs = new float[nPoints];

		int i = 0;
		for(std::vector<int>::const_iterator it = pointIdxRadiusSearch.begin();//�������н�(������)
			it != pointIdxRadiusSearch.end() ; ++it)
		{
			tempPoint = &groundPoints[*it];
			tempAng = atan2(tempPoint->y-searchPoint.y,tempPoint->x-searchPoint.x);
			if(tempAng < 0)
			{
				tempAng+=2*3.1415926;
			}
			angs[i] = tempAng;
			i++;
		}

		float angtrans;
		for(i = 0 ; i < nPoints-1 ; i++)//�������ڵĵ㰴�սǶȴ�С��������
		{
			for(int j = 0 ; j < nPoints-i-1 ; j++)	
			{
				if(angs[j]>angs[j+1])
				{
					angtrans = angs[j];
					angs[j] = angs[j+1];
					angs[j+1] = angtrans;
				}
			}
		}

		const float pi_2 = (3.1415926/2);//���μ���н�
		for(i = 0 ; i < nPoints-1 ; i++)
		{
			if((angs[i+1]-angs[i])>pi_2)
			{
				return true;
			}	
		}
		if((angs[0]-angs[nPoints-1]+4*pi_2)>pi_2)
		{
			return true;
		}

		return false;
	}


	/************************************************************************/
	/*                          angularClassifier                           */
	/************************************************************************/
	bool angularClassifier(const std::vector<LiDARBaseTools::LasPoint> &nongroundPoints,//�ǵ������
		const std::vector<LiDARBaseTools::LasPoint> &groundPoints,//�������
		const float radius,//�����뾶
		std::vector<int> &buildingIndex)//������ID
	/*
	todo:      use angularClassifier to classify building and vegetation
	in:        nonground points
	           ground points
	           search radius
	date:      2015.3.13
	author:    JianPing(lijianping@whu.edu.cn)
	see:       Classification of lidar bare-earth points,buildings,vegetation,and 
	           small objects based on region growing and angular classifier
	*/	
	{
		std::cout<<"begin angular classifier"<<std::endl;
		//1.create ground points kd-Tree
		pcl::PointCloud<pcl::PointXY>::Ptr groundcloud(new pcl::PointCloud<pcl::PointXY>);

		pcl::KdTreeFLANN<pcl::PointXY> kdTree;//�����ֶ�����

		groundcloud->resize(groundPoints.size());

		int i = 0;
		for (std::vector<LiDARBaseTools::LasPoint>::const_iterator it = groundPoints.begin();
			it!=groundPoints.end() ; ++it)
		{
			groundcloud->points[i].x = it->x;
			groundcloud->points[i].y = it->y;
			//groundcloud->points[i].z = it->z;
			i++;
		}

		kdTree.setInputCloud(groundcloud);


		//2.Neighbors within radius search

		pcl::PointXY searchPoint;//nonground Points
		std::vector<int> pointIdxRadiusSearch;
		std::vector<float> pointRadiusSquaredDistance;

		i = 0;
		for (std::vector<LiDARBaseTools::LasPoint>::const_iterator it = nongroundPoints.begin();
			it!=nongroundPoints.end() ; ++it)
		{
			searchPoint.x = it->x;
			searchPoint.y = it->y;
			//searchPoint.z = it->z;
			

			kdTree.radiusSearch(searchPoint,radius,pointIdxRadiusSearch,pointRadiusSquaredDistance);
			//std::cout<<pointIdxRadiusSearch.size()<<std::endl;
			if (pointIdxRadiusSearch.size() ==0)
			{
				buildingIndex.push_back(i);
				i++;
				continue;
			}

			if(true == pointIsBuilding(searchPoint,pointIdxRadiusSearch,pointRadiusSquaredDistance,groundPoints))
			{
				buildingIndex.push_back(i);
			}
			i++;
			pointIdxRadiusSearch.clear();
			pointRadiusSquaredDistance.clear();
		}
		std::cout<<"angular classifier done.\n"<<std::endl;
		return true;
	}

#pragma endregion

#pragma region Estimating the normals 
	/************************************************************************/
	/*                          Estimating the normals                          */
	/************************************************************************/

	struct SurfaceSegment
	{
		int SegmentID;
		std::vector<int> PointID;
		float normal_x;
		float normal_y;
		float normal_z;
		unsigned char ClassLabel;//0������1ֲ����2ɢ�ҵ���
	};

	pcl::PointCloud<pcl::PointXYZ>::Ptr m_cloud(new pcl::PointCloud<pcl::PointXYZ>);
	pcl::NormalEstimation< pcl::PointXYZ , pcl::Normal> m_ne;
	pcl::search::KdTree<pcl::PointXYZ>::Ptr m_tree(new pcl::search::KdTree<pcl::PointXYZ>);
	pcl::PointCloud<pcl::Normal>::Ptr m_cloud_normals(new pcl::PointCloud<pcl::Normal>);

	const int K = 20;

	std::deque<int> m_seedPointdeque;//���ӵ㣨���в�����

	std::vector<SurfaceSegment> m_SurfaceSeg;//���ֵ���

	bool getSeedPointsIndex(const float searchRadius)
	/*
	todo:      �õ����������ӵ㣨���������ڵ㵽�����Ĳв�ķ������򣩣�����seedPointdeque
	date:      2015.3.23
	author:    JianPing(lijianping@whu.edu.cn)
	see:       Area-wide roof plane segmentation in airborne
	           LiDAR point clouds(2012)
	*/	
	{
		std::cout<<"begin to get seed"<<std::endl;
		float *roughness = new float[m_cloud->points.size()];//�в�
		std::vector<int> pointIdxKNNSearch(K);
		std::vector<float> pointKNNSquaredDistance(K);
		pcl::PointXYZ searchPoint;
		pcl::Normal searchPointNormal;
		std::vector <double> difDis;//����������searchRadius�ڵĵ�Ĳв�
		/***����ÿ��������в�***/
		for (int i = 0 ; i < m_cloud->points.size() ; i++)
		{
			searchPoint = m_cloud->points[i];
			searchPointNormal = m_cloud_normals->points[i];
			m_tree->nearestKSearch(searchPoint,K,pointIdxKNNSearch,pointKNNSquaredDistance);
			for (int j = 0 ; j < K ; j++)
			{
				if (pointKNNSquaredDistance[j] <= searchRadius)
				{
					/*��������㵽���ӵ�����ƽ��Ĵ�ֱ����*/
					float x1,y1,z1;
					double dis;
					//std::cout<<pointIdxKNNSearch[j]<<"\n";
					x1 = m_cloud->points[pointIdxKNNSearch[j]].x;
					y1 = m_cloud->points[pointIdxKNNSearch[j]].y;
					z1 = m_cloud->points[pointIdxKNNSearch[j]].z;//ƽ����һ�㣨x1,y1,z1��
					//std::cout<<x1<<" "<<y1<<" "<<z1<<"\n";
					double g;
					g = sqrt(searchPointNormal.normal_x*searchPointNormal.normal_x+
						searchPointNormal.normal_y*searchPointNormal.normal_y+
						searchPointNormal.normal_z*searchPointNormal.normal_z);//��ƽ�淽��ϵ��nx,ny,nz��ƽ���͵Ŀ�ƽ��
					double f1 = searchPointNormal.normal_x*(x1-searchPoint.x);
					double f2 = searchPointNormal.normal_x*(y1-searchPoint.y);
					double f3 = searchPointNormal.normal_x*(z1-searchPoint.z);
					double f = fabs(f1 + f2 + f3);
					dis = (f/g);
					difDis.push_back(dis);
				}
			}
			/***����в�ķ���***/
			double average(0),s(0);
			for (std::vector<double>::iterator it = difDis.begin() ; it!=difDis.end() ; it++)
			{
				average+=*it;
			}
			average/=difDis.size();
			for (std::vector<double>::iterator it = difDis.begin() ; it!=difDis.end() ; it++)
			{
				s+=(fabs(*it - average)*fabs(*it - average));
			}
			if (difDis.size() == 0)//����һ��
			{
				roughness[i] = 99999;
			}
			else
			{
				roughness[i] = sqrt(s/difDis.size());
			}
			

			/***����***/
			difDis.clear();
			pointIdxKNNSearch.clear();
			pointKNNSquaredDistance.clear();

		}
		/***roughness���մ�С��������***/
		int *pointIdx = new int[m_cloud->points.size()];
		for (int i = 0 ; i < m_cloud->points.size() ; i++)
		{
			pointIdx[i] = i;//��ʼ��
		}
		float ftemp;int ntemp;
		for (int i = 0 ; i < m_cloud->points.size()-1 ; i++)
		{
			for (int j = 0 ; j < m_cloud->points.size()-i-1 ; j++)
			{
				if (roughness[j]>roughness[j+1])
				{
					ftemp = roughness[j];
					roughness[j] = roughness[j+1];
					roughness[j+1] = ftemp;

					ntemp = pointIdx[j];
					pointIdx[j] = pointIdx[j+1];
					pointIdx[j+1] = ntemp;
				}
			}
		}

		for (int i = 0 ; i < m_cloud->points.size() ; i++)
		{
			m_seedPointdeque.push_back(pointIdx[i]);
		}


		delete [] pointIdx;
		delete [] roughness;

		return true;
	}

	bool pointSegmentaionCore(const float alpha , const float dist)
	/*
	todo:      ���ӵ�����
	date:      2015.3.24
	author:    JianPing(lijianping@whu.edu.cn)
	see:       Area-wide roof plane segmentation in airborne
	           LiDAR point clouds(2012)
	*/	
	{
		float cosT = cos(alpha*3.14159/180.);
		std::cout<<"cosT: "<<cosT<<std::endl;
		std::cout<<"dist: "<<dist<<std::endl;

		/***��¼���Ƿ����***/
		int *pointClassLabel = new int[m_cloud->points.size()];
		for (int i = 0 ; i < m_cloud->points.size() ; i++)
		{
			pointClassLabel[i] = -1;//��ʼ��Ϊδ���
		}

		SurfaceSegment tempSeg;

		pcl::PointXYZ searchPoint;
		pcl::Normal searchPointNormal;
		std::vector<int>pointIdxSearch;
		std::vector<float>pointNKNSquaredDistance;
		/*��������deque�����ڴ洢һ���ָ���������ӵ�*/
		std::deque <int> tempseed;
		

		while(!m_seedPointdeque.empty())
		{
			int tempIdx = m_seedPointdeque.front();
			m_seedPointdeque.pop_front();
			if ( pointClassLabel[tempIdx]!=-1 )
			{
				continue;
			}

			tempseed.push_back(tempIdx);
			tempSeg.PointID.push_back(tempIdx);

			while(!tempseed.empty())
			{
				tempIdx = tempseed.front();
				tempseed.pop_front();
				searchPoint = m_cloud->points[tempIdx];
				searchPointNormal = m_cloud_normals->points[tempIdx];

				int N = m_tree->nearestKSearch(searchPoint,K,pointIdxSearch,pointNKNSquaredDistance);

				for (int i = 1 ; i < N ; i++)
				{
					if (pointClassLabel[pointIdxSearch[i]] == -1)
					{
						/*�������������ӵ㷨�����ļн�*/
						float nx1 = m_cloud_normals->points[pointIdxSearch[i]].normal_x;
						float ny1 = m_cloud_normals->points[pointIdxSearch[i]].normal_y;
						float nz1 = m_cloud_normals->points[pointIdxSearch[i]].normal_z;
						float n_n1 = searchPointNormal.normal_x*nx1+
							searchPointNormal.normal_y*ny1+
							searchPointNormal.normal_z*nz1;
						float n_n = sqrt(searchPointNormal.normal_x*searchPointNormal.normal_x+
							searchPointNormal.normal_y*searchPointNormal.normal_y+
							searchPointNormal.normal_z*searchPointNormal.normal_z);
						float n1_n1 = sqrt(nx1*nx1+ny1*ny1+nz1*nz1);
						float Cosnormal = abs(n_n1/(n_n*n1_n1));

						/*��������㵽���ӵ�����ƽ��Ĵ�ֱ����*/
						float x1,y1,z1;double dis;
						x1 = m_cloud->points[pointIdxSearch[i]].x;
						y1 = m_cloud->points[pointIdxSearch[i]].y;
						z1 = m_cloud->points[pointIdxSearch[i]].z;//ƽ����һ�㣨x1,y1,z1��
						double g;
						g = sqrt(searchPointNormal.normal_x*searchPointNormal.normal_x+
							searchPointNormal.normal_y*searchPointNormal.normal_y+
							searchPointNormal.normal_z*searchPointNormal.normal_z);//��ƽ�淽��ϵ��nx,ny,nz��ƽ���͵Ŀ�ƽ��
						double f1 = searchPointNormal.normal_x*(x1-searchPoint.x);
						double f2 = searchPointNormal.normal_y*(y1-searchPoint.y);
						double f3 = searchPointNormal.normal_z*(z1-searchPoint.z);
						double f = fabs(f1 + f2 + f3);
						dis = (f/g);

						if (Cosnormal > cosT && fabs(dis) < dist)//��״������������
						{
							tempSeg.PointID.push_back(pointIdxSearch[i]);
							tempseed.push_back(pointIdxSearch[i]);
							pointClassLabel[pointIdxSearch[i]] = 0;
						}
					}
				}
				pointIdxSearch.clear();
				pointNKNSquaredDistance.clear();
			}

			if (tempSeg.PointID.size() < 5)//��������
			{
				for (int i = 0 ; i < tempSeg.PointID.size() ; i++)
				{
					pointClassLabel[tempSeg.PointID[i]] = -1;
				}
				tempSeg.PointID.clear();
				tempseed.clear();
				continue;
			}
			m_SurfaceSeg.push_back(tempSeg);
			tempSeg.PointID.clear();
			tempseed.clear();

		}

		/***����***/
		delete []pointClassLabel;
		return true;
	}

	//main
	bool pointNormalSegmentation(const std::vector<LiDARBaseTools::LasPoint> &nongroundPoints,
		const float searchRadius,
		const float alpha,
		const float dist)
	{
		std::cout<<"begin pointNormalSegmentation"<<std::endl;
		m_cloud->points.resize(nongroundPoints.size());

		int i = 0;
		for (std::vector<LiDARBaseTools::LasPoint>::const_iterator it = nongroundPoints.begin();
			it!= nongroundPoints.end();it++)
		{
			m_cloud->points[i].x = it->x;
			m_cloud->points[i].y = it->y;
			m_cloud->points[i].z = it->z;
			i++;
		}

		m_tree->setInputCloud(m_cloud);

		m_ne.setInputCloud(m_cloud);
		m_ne.setSearchMethod(m_tree);
		m_ne.setRadiusSearch(searchRadius);
		m_ne.compute(*m_cloud_normals);

		/***�õ�roughness��С���������seedPointdeque***/
		getSeedPointsIndex(searchRadius);

		/***������***/
		pointSegmentaionCore(alpha,dist);



		std::cout<<"success pointNormalSegmentation"<<std::endl;
		return true;
	}

#pragma endregion
}

#endif